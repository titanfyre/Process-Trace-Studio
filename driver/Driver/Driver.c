#include <ntifs.h>
#include <wdf.h>
#include <initguid.h>
#include "ProcessTrace.h"

DEFINE_GUID(GUID_DEVINTERFACE_TITAN_PROCESS_TRACE,0x8076b940,0x50ad,0x4546,0xbd,0x1a,0x7e,0xc5,0xa8,0x1c,0x31,0x27);
C_ASSERT(sizeof(PT_EVENT)==584);
C_ASSERT(sizeof(PT_STATS)==88);
C_ASSERT(sizeof(PT_BATCH)==9448);
typedef struct PT_CONTEXT {
    WDFSPINLOCK lock;
    WDFWAITLOCK management;
    WDFQUEUE reads;
    WDFFILEOBJECT owner;
    EX_RUNDOWN_REF rundown;
    volatile LONG active;
    WDFREQUEST pending;
    WDFFILEOBJECT pending_owner;
    PT_U64 pending_session;
    volatile LONG cleaned;
    BOOLEAN process_registered, image_registered;
    PT_U32 head, count;
    PT_STATS stats;
    PT_EVENT ring[PT_RING_CAPACITY];
} PT_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PT_CONTEXT, Context);
typedef struct FILE_CONTEXT { BOOLEAN closing; } FILE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_CONTEXT, FileContext);
typedef struct REQUEST_CONTEXT { PT_U64 session; } REQUEST_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(REQUEST_CONTEXT, RequestContext);
static PT_CONTEXT* volatile g_context;
static volatile LONG g_claimed;
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD AddDevice;
EVT_WDF_OBJECT_CONTEXT_CLEANUP CleanupDevice;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT InitIo;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP CleanupIo;
EVT_WDF_DEVICE_FILE_CREATE FileCreate;
EVT_WDF_FILE_CLEANUP FileCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL IoControl;
EVT_WDF_IO_QUEUE_IO_CANCELED_ON_QUEUE ReadCanceled;

static VOID SnapshotLocked(PT_CONTEXT* c, PT_STATS* s) {
    *s=c->stats;s->active=(PT_U32)c->active;s->queued=c->count;
}
static VOID DrainLocked(PT_CONTEXT* c, PT_BATCH* b) {
    PT_RingDrain(c->ring,&c->head,&c->count,&c->stats,b);b->stats.active=(PT_U32)c->active;
}
static BOOLEAN CurrentLocked(PT_CONTEXT* c,WDFREQUEST request) {
    WDFFILEOBJECT file=WdfRequestGetFileObject(request);
    return file&&c->owner==file&&!FileContext(file)->closing&&!c->cleaned;
}
static VOID ClearPending(PT_CONTEXT* c,WDFREQUEST request) {
    WdfSpinLockAcquire(c->lock);if(c->pending==request)c->pending=NULL;WdfSpinLockRelease(c->lock);
}
// Queue operations can dispatch source/cancel callbacks inline. No lifecycle
// lock is held here, while forwarding, or while completing any request.
static VOID WakeReader(PT_CONTEXT* c) {
    WDFREQUEST target=NULL,found=NULL,request=NULL;
    PT_BATCH* out=NULL;NTSTATUS status;BOOLEAN ready;ULONG_PTR bytes=0;
    WdfSpinLockAcquire(c->lock);
    target=c->pending;
    if(target)WdfObjectReference(target);
    WdfSpinLockRelease(c->lock);
    if(!target)return;
    status=WdfIoQueueFindRequest(c->reads,NULL,NULL,NULL,&found);
    if(!NT_SUCCESS(status)){WdfObjectDereference(target);return;}
    if(found!=target){WdfObjectDereference(found);WdfObjectDereference(target);return;}
    WdfSpinLockAcquire(c->lock);
    // A queue cancellation may complete target after FindRequest. Read only
    // our own identity snapshot until RetrieveFoundRequest grants ownership;
    // object references alone do not permit APIs on a completed WDFREQUEST.
    ready=c->pending==target&&(c->cleaned||c->owner!=c->pending_owner||!c->active||c->pending_session!=c->stats.session||c->count!=0);
    WdfSpinLockRelease(c->lock);
    if(ready)status=WdfIoQueueRetrieveFoundRequest(c->reads,found,&request);
    else status=STATUS_NO_MORE_ENTRIES;
    WdfObjectDereference(found);
    if(NT_SUCCESS(status)){
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(*out),(PVOID*)&out,NULL);
        WdfSpinLockAcquire(c->lock);
        if(c->pending==request)c->pending=NULL;
        if(NT_SUCCESS(status)){
            if(!CurrentLocked(c,request)||!c->active||RequestContext(request)->session!=c->stats.session)status=STATUS_CANCELLED;
            else{DrainLocked(c,out);bytes=sizeof(*out);}
        }
        WdfSpinLockRelease(c->lock);
        WdfRequestCompleteWithInformation(request,status,bytes);
    }
    WdfObjectDereference(target);
}
static VOID CopyName(PT_EVENT* e, PCUNICODE_STRING source, PT_U32 options) {
    if(!source || !source->Buffer || (source->Length&1))return;
    PT_CopyName(e,(const PT_U16*)source->Buffer,source->Length/sizeof(WCHAR),options);
}
static VOID Push(PT_CONTEXT* c, PT_EVENT* e, PT_U64 session) {
    WdfSpinLockAcquire(c->lock);
    if(!c->active||c->stats.session!=session){WdfSpinLockRelease(c->lock);return;}
    PT_RingPush(c->ring,&c->head,&c->count,&c->stats,e);
    WdfSpinLockRelease(c->lock);WakeReader(c);
}
static PT_CONTEXT* Enter(PT_U32* options, PT_U64* session) {
    PT_CONTEXT* c=(PT_CONTEXT*)InterlockedCompareExchangePointer((PVOID volatile*)&g_context,NULL,NULL);
    if(!c || !ExAcquireRundownProtection(&c->rundown))return NULL;
    if(!InterlockedCompareExchange(&c->active,0,0)){ExReleaseRundownProtection(&c->rundown);return NULL;}
    WdfSpinLockAcquire(c->lock);*options=c->stats.options;*session=c->stats.session;WdfSpinLockRelease(c->lock);
    return c;
}
static VOID ProcessNotify(PEPROCESS process,HANDLE pid,PPS_CREATE_NOTIFY_INFO info) {
    PT_U32 options=0;
    PT_U64 session=0;
    PT_CONTEXT* c=Enter(&options,&session);
    PT_EVENT e;
    PEPROCESS parent;
    if(!c)return;
    RtlZeroMemory(&e,sizeof(e));e.qpc=(PT_U64)KeQueryPerformanceCounter(NULL).QuadPart;
    e.pid=HandleToULong(pid);e.process_key=PsGetProcessStartKey(process);
    e.create_time_100ns=(PT_U64)PsGetProcessCreateTimeQuadPart(process);
    if(info){
        e.type=PT_EVENT_CREATE;e.parent_pid=HandleToULong(info->ParentProcessId);
        if(NT_SUCCESS(PsLookupProcessByProcessId(info->ParentProcessId,&parent))){e.parent_key=PsGetProcessStartKey(parent);e.flags|=PT_FLAG_PARENT_OBSERVED;ObDereferenceObject(parent);}
        CopyName(&e,info->ImageFileName,options);
    }else{e.type=PT_EVENT_EXIT;e.exit_status=(PT_U32)PsGetProcessExitStatus(process);}
    Push(c,&e,session);ExReleaseRundownProtection(&c->rundown);
}
static VOID ImageNotify(PUNICODE_STRING name,HANDLE pid,PIMAGE_INFO image) {
    PT_U32 options=0;
    PT_U64 session=0;
    PT_CONTEXT* c=Enter(&options,&session);
    PT_EVENT e;
    PEPROCESS process;
    if(!c)return;
    if(!(options&PT_CAPTURE_IMAGES)){ExReleaseRundownProtection(&c->rundown);return;}
    RtlZeroMemory(&e,sizeof(e));e.type=PT_EVENT_IMAGE;e.pid=HandleToULong(pid);
    e.qpc=(PT_U64)KeQueryPerformanceCounter(NULL).QuadPart;
    if(image->SystemModeImage)e.flags|=PT_FLAG_KERNEL_IMAGE;
    else if(NT_SUCCESS(PsLookupProcessByProcessId(pid,&process))){
        e.process_key=PsGetProcessStartKey(process);e.create_time_100ns=(PT_U64)PsGetProcessCreateTimeQuadPart(process);ObDereferenceObject(process);
    }else e.flags|=PT_FLAG_IDENTITY_UNKNOWN;
    CopyName(&e,name,options);Push(c,&e,session);ExReleaseRundownProtection(&c->rundown);
}
static VOID StopLocked(PT_CONTEXT* c, WDFFILEOBJECT owner) {
    WdfSpinLockAcquire(c->lock);
    if(c->owner==owner){InterlockedExchange(&c->active,0);c->stats.active=0;c->head=c->count=0;}
    WdfSpinLockRelease(c->lock);
}
VOID FileCreate(WDFDEVICE device,WDFREQUEST request,WDFFILEOBJECT file) {
    PT_CONTEXT* c=Context(device);NTSTATUS status=STATUS_SUCCESS;
    WdfWaitLockAcquire(c->management,NULL);
    WdfSpinLockAcquire(c->lock);
    if(c->cleaned)status=STATUS_DELETE_PENDING;else if(c->owner)status=STATUS_SHARING_VIOLATION;else c->owner=file;
    WdfSpinLockRelease(c->lock);WdfWaitLockRelease(c->management);WdfRequestComplete(request,status);
}
VOID FileCleanup(WDFFILEOBJECT file) {
    PT_CONTEXT* c=Context(WdfFileObjectGetDevice(file));BOOLEAN owns;
    WdfWaitLockAcquire(c->management,NULL);
    WdfSpinLockAcquire(c->lock);FileContext(file)->closing=TRUE;owns=c->owner==file;WdfSpinLockRelease(c->lock);
    if(owns){StopLocked(c,file);WdfSpinLockAcquire(c->lock);if(c->owner==file)c->owner=NULL;WdfSpinLockRelease(c->lock);}
    WdfWaitLockRelease(c->management);WakeReader(c);
}
VOID ReadCanceled(WDFQUEUE queue,WDFREQUEST request) {
    PT_CONTEXT* c=Context(WdfIoQueueGetDevice(queue));
    ClearPending(c,request);WdfRequestComplete(request,STATUS_CANCELLED);
}
typedef struct COMPLETION { NTSTATUS status;ULONG_PTR bytes;BOOLEAN forward,wake; } COMPLETION;
static COMPLETION Result(NTSTATUS status,ULONG_PTR bytes) {COMPLETION result={0};result.status=status;result.bytes=bytes;return result;}
static COMPLETION IoControlLocked(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code) {
    PT_CONTEXT* c=Context(WdfIoQueueGetDevice(queue));
    PT_REQUEST* r;PT_REQUEST copy;PT_STATS* stats;PT_BATCH* batch;
    NTSTATUS status;ULONG_PTR bytes=0;BOOLEAN owned;COMPLETION result;
    if(code!=PT_START&&code!=PT_STOP&&code!=PT_STATUS&&code!=PT_RECEIVE)return Result(STATUS_INVALID_DEVICE_REQUEST,0);
    if(input!=sizeof(copy))return Result(STATUS_INFO_LENGTH_MISMATCH,0);
    status=WdfRequestRetrieveInputBuffer(request,sizeof(copy),(PVOID*)&r,NULL);
    if(!NT_SUCCESS(status))return Result(status,0);copy=*r;
    if(!PT_ValidRequest(&copy,code))return Result(STATUS_INVALID_PARAMETER,0);
    WdfSpinLockAcquire(c->lock);owned=CurrentLocked(c,request);WdfSpinLockRelease(c->lock);
    if(!owned)return Result(STATUS_ACCESS_DENIED,0);
    if(code==PT_START||code==PT_STOP){
        if(output)return Result(STATUS_INFO_LENGTH_MISMATCH,0);
        StopLocked(c,WdfRequestGetFileObject(request));
        if(code==PT_START){LARGE_INTEGER frequency,utc;LARGE_INTEGER now=KeQueryPerformanceCounter(&frequency);KeQuerySystemTimePrecise(&utc);
            int committed;
            WdfSpinLockAcquire(c->lock);committed=PT_CommitCapture(&c->stats,c->owner==WdfRequestGetFileObject(request),FileContext(WdfRequestGetFileObject(request))->closing,c->cleaned,copy.flags,(PT_U64)frequency.QuadPart,(PT_U64)now.QuadPart,(PT_U64)utc.QuadPart);if(committed)InterlockedExchange(&c->active,1);WdfSpinLockRelease(c->lock);
            if(!committed){result=Result(STATUS_DELETE_PENDING,0);result.wake=TRUE;return result;}
        }
        result=Result(STATUS_SUCCESS,0);result.wake=TRUE;return result;
    }else if(code==PT_STATUS){
        if(output!=sizeof(*stats))return Result(STATUS_INFO_LENGTH_MISMATCH,0);
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(*stats),(PVOID*)&stats,NULL);
        if(NT_SUCCESS(status)){WdfSpinLockAcquire(c->lock);SnapshotLocked(c,stats);WdfSpinLockRelease(c->lock);bytes=sizeof(*stats);}
    }else{
        if(output!=sizeof(*batch))return Result(STATUS_INFO_LENGTH_MISMATCH,0);
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(*batch),(PVOID*)&batch,NULL);
        if(!NT_SUCCESS(status))return Result(status,0);
        WdfSpinLockAcquire(c->lock);
        if(c->pending){WdfSpinLockRelease(c->lock);return Result(STATUS_DEVICE_BUSY,0);}
        if(c->count || !c->active){DrainLocked(c,batch);WdfSpinLockRelease(c->lock);return Result(STATUS_SUCCESS,sizeof(*batch));}
        RequestContext(request)->session=c->stats.session;c->pending=request;c->pending_owner=WdfRequestGetFileObject(request);c->pending_session=c->stats.session;
        WdfSpinLockRelease(c->lock);
        result=Result(STATUS_SUCCESS,0);result.forward=TRUE;return result;
    }
    return Result(status,bytes);
}
VOID IoControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code) {
    PT_CONTEXT* c=Context(WdfIoQueueGetDevice(queue));
    COMPLETION result;
    WdfWaitLockAcquire(c->management,NULL);result=IoControlLocked(queue,request,output,input,code);WdfWaitLockRelease(c->management);
    if(result.forward){
        // Keep identity valid across inline cancellation/completion. Ownership
        // still belongs to WDF after a successful forward; WakeReader reacquires
        // it through FindRequest/RetrieveFoundRequest before touching outputs.
        WdfObjectReference(request);
        result.status=WdfRequestForwardToIoQueue(request,c->reads);
        if(NT_SUCCESS(result.status))WakeReader(c);
        else{ClearPending(c,request);WdfRequestComplete(request,result.status);}
        WdfObjectDereference(request);return;
    }
    if(result.wake)WakeReader(c);
    WdfRequestCompleteWithInformation(request,result.status,result.bytes);
}
VOID CleanupDevice(WDFOBJECT object) {
    UNREFERENCED_PARAMETER(object);
    InterlockedExchange(&g_claimed,0);
}
_Use_decl_annotations_ VOID CleanupIo(WDFDEVICE object) {
    PT_CONTEXT* c=Context(object);
    WdfWaitLockAcquire(c->management,NULL);
    if(InterlockedExchange(&c->cleaned,1)){WdfWaitLockRelease(c->management);return;}
    WdfSpinLockAcquire(c->lock);InterlockedExchange(&c->active,0);c->stats.active=0;WdfSpinLockRelease(c->lock);
    WdfWaitLockRelease(c->management);
    InterlockedCompareExchangePointer((PVOID volatile*)&g_context,NULL,c);
    if(c->process_registered)PsSetCreateProcessNotifyRoutineEx(ProcessNotify,TRUE);
    if(c->image_registered)PsRemoveLoadImageNotifyRoutine(ImageNotify);
    ExWaitForRundownProtectionRelease(&c->rundown);
    if(c->reads){WakeReader(c);WdfIoQueuePurgeSynchronously(c->reads);}
}
_Use_decl_annotations_ NTSTATUS InitIo(WDFDEVICE device) {
    PT_CONTEXT* c=Context(device);NTSTATUS status;
    status=PsSetCreateProcessNotifyRoutineEx(ProcessNotify,FALSE);if(!NT_SUCCESS(status))return status;c->process_registered=TRUE;
    status=PsSetLoadImageNotifyRoutine(ImageNotify);
    if(!NT_SUCCESS(status)){PsSetCreateProcessNotifyRoutineEx(ProcessNotify,TRUE);c->process_registered=FALSE;return status;}
    c->image_registered=TRUE;InterlockedExchangePointer((PVOID volatile*)&g_context,c);return STATUS_SUCCESS;
}
NTSTATUS AddDevice(WDFDRIVER driver,PWDFDEVICE_INIT init) {
    WDFDEVICE device;WDF_OBJECT_ATTRIBUTES attrs,queueAttrs;
    WDF_FILEOBJECT_CONFIG files;WDF_IO_QUEUE_CONFIG queue;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    PT_CONTEXT* c;NTSTATUS status;UNICODE_STRING sddl;
    UNREFERENCED_PARAMETER(driver);
    if(InterlockedCompareExchange(&g_claimed,1,0))return STATUS_DEVICE_BUSY;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);pnp.EvtDeviceSelfManagedIoInit=InitIo;pnp.EvtDeviceSelfManagedIoCleanup=CleanupIo;WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WdfDeviceInitSetDeviceType(init,0x8000);WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN|FILE_AUTOGENERATED_DEVICE_NAME,FALSE);
    RtlInitUnicodeString(&sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    status=WdfDeviceInitAssignSDDLString(init,&sddl);if(!NT_SUCCESS(status)){InterlockedExchange(&g_claimed,0);return status;}
    WDF_FILEOBJECT_CONFIG_INIT(&files,FileCreate,NULL,FileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs,FILE_CONTEXT);attrs.ExecutionLevel=WdfExecutionLevelPassive;
    WdfDeviceInitSetFileObjectConfig(init,&files,&attrs);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs,REQUEST_CONTEXT);WdfDeviceInitSetRequestAttributes(init,&attrs);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs,PT_CONTEXT);attrs.EvtCleanupCallback=CleanupDevice;attrs.ExecutionLevel=WdfExecutionLevelPassive;
    status=WdfDeviceCreate(&init,&attrs,&device);if(!NT_SUCCESS(status)){InterlockedExchange(&g_claimed,0);return status;}
    c=Context(device);ExInitializeRundownProtection(&c->rundown);
    c->stats.version=PT_VERSION;c->stats.size=sizeof(PT_STATS);c->stats.capacity=PT_RING_CAPACITY;
    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);attrs.ParentObject=device;
    status=WdfSpinLockCreate(&attrs,&c->lock);if(!NT_SUCCESS(status))return status;
    status=WdfWaitLockCreate(&attrs,&c->management);if(!NT_SUCCESS(status))return status;
    WDF_IO_QUEUE_CONFIG_INIT(&queue,WdfIoQueueDispatchManual);queue.PowerManaged=WdfFalse;queue.EvtIoCanceledOnQueue=ReadCanceled;
    WDF_OBJECT_ATTRIBUTES_INIT(&queueAttrs);queueAttrs.ExecutionLevel=WdfExecutionLevelDispatch;
    status=WdfIoQueueCreate(device,&queue,&queueAttrs,&c->reads);if(!NT_SUCCESS(status))return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);queue.EvtIoDeviceControl=IoControl;queue.PowerManaged=WdfFalse;
    status=WdfIoQueueCreate(device,&queue,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(status))return status;
    status=WdfDeviceCreateDeviceInterface(device,&GUID_DEVINTERFACE_TITAN_PROCESS_TRACE,NULL);if(!NT_SUCCESS(status))return status;
    return STATUS_SUCCESS;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry) {
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,AddDevice);
    return WdfDriverCreate(object,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
