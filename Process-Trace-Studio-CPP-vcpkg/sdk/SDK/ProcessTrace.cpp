#include "ProcessTrace.h"
#include <setupapi.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>
namespace {
const GUID Interface={0x8076b940,0x50ad,0x4546,{0xbd,0x1a,0x7e,0xc5,0xa8,0x1c,0x31,0x27}};
struct EventHandle { HANDLE h=CreateEventW(nullptr,TRUE,FALSE,nullptr); ~EventHandle(){if(h)CloseHandle(h);} };
std::string Utf8(const std::wstring& s){
    if(s.empty())return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(static_cast<size_t>(n),0);if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);return out;
}
[[noreturn]] void Fail(DWORD code){throw std::runtime_error(Utf8(ProcessTrace::Error(code)));}
std::string Quote(std::wstring value){
    // Excel/LibreOffice treat quoted formula-looking values as formulas too.
    if(!value.empty()&&(value[0]==L'='||value[0]==L'+'||value[0]==L'-'||value[0]==L'@'||value[0]==L'\t'||value[0]==L'\r'))value.insert(value.begin(),L'\'');
    std::string out="\"";for(char c:Utf8(value)){if(c=='\"')out+='\"';out+=c;}return out+'\"';
}
}
std::wstring ProcessTrace::Error(unsigned long code){
    wchar_t* message=nullptr;
    DWORD n=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),reinterpret_cast<LPWSTR>(&message),0,nullptr);
    std::wstring out=n?message:L"The requested driver operation failed.";if(message)LocalFree(message);
    while(!out.empty()&&(out.back()==L'\r'||out.back()==L'\n'))out.pop_back();
    if(code==ERROR_FILE_NOT_FOUND||code==ERROR_NO_MORE_ITEMS)out=L"The Process Trace driver interface is unavailable. Install the signed driver and restart only if setup requests it.";
    if(code==ERROR_ACCESS_DENIED)out=L"Access denied. Start this application as administrator; the device is restricted to SYSTEM and administrators.";
    if(code==ERROR_SHARING_VIOLATION)out=L"Another Process Trace client owns the capture device. Close that client first.";
    return out+L" (Windows error "+std::to_wstring(code)+L")";
}
bool ProcessTrace::Valid(const PT_STATS& s) noexcept {
    return s.version==PT_VERSION&&s.size==sizeof(s)&&s.active<=1&&!(s.options&~(PT_CAPTURE_IMAGES|PT_FULL_PATHS))&&s.capacity==PT_RING_CAPACITY&&s.queued<=s.capacity&&s.dropped<=s.received&&s.truncated<=s.received&&s.reserved[0]==0&&s.reserved[1]==0&&(!s.active||(s.session&&s.frequency&&s.start_qpc));
}
bool ProcessTrace::Valid(const PT_BATCH& b) noexcept {
    if(!Valid(b.stats)||b.count>PT_BATCH_MAX||b.reserved[0]||b.reserved[1]||b.reserved[2])return false;
    PT_U64 previous=0;
    for(PT_U32 i=0;i<b.count;++i){const auto& e=b.events[i];
        if(!e.sequence||e.sequence<=previous||e.sequence>b.stats.received||e.name_chars>=PT_NAME_CHARS||e.name[e.name_chars]||e.type<PT_EVENT_CREATE||e.type>PT_EVENT_IMAGE||(e.flags&~(PT_FLAG_TRUNCATED|PT_FLAG_KERNEL_IMAGE|PT_FLAG_IDENTITY_UNKNOWN|PT_FLAG_PARENT_OBSERVED)))return false;
        if(e.type!=PT_EVENT_IMAGE&&(e.flags&(PT_FLAG_KERNEL_IMAGE|PT_FLAG_IDENTITY_UNKNOWN)))return false;
        if(e.type!=PT_EVENT_CREATE&&(e.flags&PT_FLAG_PARENT_OBSERVED))return false;
        if(!(e.flags&(PT_FLAG_KERNEL_IMAGE|PT_FLAG_IDENTITY_UNKNOWN))&&!e.process_key)return false;
        previous=e.sequence;
    }
    return true;
}
std::wstring ProcessTrace::Name(const PT_EVENT& e){
    if(e.name_chars>=PT_NAME_CHARS)throw std::invalid_argument("Malformed event name length.");
    std::wstring out;out.reserve(e.name_chars);for(PT_U32 i=0;i<e.name_chars;++i)out.push_back(static_cast<wchar_t>(e.name[i]));return out;
}
std::string ProcessTrace::Csv(const PT_BATCH& b){
    if(!Valid(b))throw std::invalid_argument("Malformed Process Trace batch.");
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(3);
    for(PT_U32 i=0;i<b.count;++i){const auto& e=b.events[i];
        const double elapsed=e.qpc>=b.stats.start_qpc&&b.stats.frequency?static_cast<double>(e.qpc-b.stats.start_qpc)*1000.0/static_cast<double>(b.stats.frequency):0;
        out<<b.stats.session<<','<<e.sequence<<','<<elapsed<<','<<(e.type==PT_EVENT_CREATE?"create":e.type==PT_EVENT_EXIT?"exit":"image")<<','<<e.pid<<','<<e.process_key<<','<<e.create_time_100ns<<','<<e.parent_pid<<','<<e.parent_key<<','<<e.exit_status<<','<<e.flags<<','<<Quote(Name(e))<<"\r\n";
    }
    return out.str();
}
ProcessTrace::Client::Client(){
    HDEVINFO list=SetupDiGetClassDevsW(&Interface,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(list==INVALID_HANDLE_VALUE)Fail(GetLastError());
    struct Cleanup{HDEVINFO h;~Cleanup(){SetupDiDestroyDeviceInfoList(h);}} cleanup{list};
    SP_DEVICE_INTERFACE_DATA item{};item.cbSize=sizeof(item);
    if(!SetupDiEnumDeviceInterfaces(list,nullptr,&Interface,0,&item))Fail(GetLastError());
    DWORD needed=0;SetupDiGetDeviceInterfaceDetailW(list,&item,nullptr,0,&needed,nullptr);
    if(needed<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||needed>65536)Fail(ERROR_INVALID_DATA);
    std::vector<unsigned char> buffer(needed);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());detail->cbSize=sizeof(*detail);
    if(!SetupDiGetDeviceInterfaceDetailW(list,&item,detail,needed,nullptr,nullptr))Fail(GetLastError());
    device_=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
    if(device_==INVALID_HANDLE_VALUE)Fail(GetLastError());
    try{auto s=Status();if(!Valid(s))Fail(ERROR_INVALID_DATA);}catch(...){CloseHandle(device_);device_=INVALID_HANDLE_VALUE;throw;}
}
ProcessTrace::Client::~Client(){if(device_!=INVALID_HANDLE_VALUE){Cancel();CloseHandle(device_);}}
void ProcessTrace::Client::Command(PT_U32 code,PT_U32 flags,void* output,DWORD bytes) const {
    std::lock_guard<std::mutex> guard(control_);PT_REQUEST request{};request.version=PT_VERSION;request.size=sizeof(request);request.flags=flags;
    EventHandle done;if(!done.h)Fail(GetLastError());OVERLAPPED op{};op.hEvent=done.h;DWORD got=0;
    if(!DeviceIoControl(device_,code,&request,sizeof(request),output,bytes,&got,&op)){
        DWORD error=GetLastError();if(error!=ERROR_IO_PENDING)Fail(error);
        DWORD wait=WaitForSingleObject(done.h,5000);
        if(wait!=WAIT_OBJECT_0){
            CancelIoEx(device_,&op);
            if(!GetOverlappedResult(device_,&op,&got,TRUE))Fail(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_GEN_FAILURE);
        }else if(!GetOverlappedResult(device_,&op,&got,FALSE))Fail(GetLastError());
    }
    if(got!=bytes)Fail(ERROR_INVALID_DATA);
}
void ProcessTrace::Client::Start(bool images,bool fullPaths){Command(PT_START,(images?PT_CAPTURE_IMAGES:0u)|(fullPaths?PT_FULL_PATHS:0u),nullptr,0);}
void ProcessTrace::Client::Stop(){Command(PT_STOP,0,nullptr,0);}
PT_STATS ProcessTrace::Client::Status() const {PT_STATS stats{};Command(PT_STATUS,0,&stats,sizeof(stats));if(!Valid(stats))Fail(ERROR_INVALID_DATA);return stats;}
void ProcessTrace::Client::Cancel() noexcept{if(device_!=INVALID_HANDLE_VALUE)CancelIoEx(device_,nullptr);}
PT_BATCH ProcessTrace::Client::Receive(DWORD timeout,HANDLE stop){
    PT_BATCH out{};PT_REQUEST request{};request.version=PT_VERSION;request.size=sizeof(request);
    EventHandle done;if(!done.h)Fail(GetLastError());OVERLAPPED op{};op.hEvent=done.h;DWORD got=0;
    if(!DeviceIoControl(device_,PT_RECEIVE,&request,sizeof(request),&out,sizeof(out),&got,&op)){
        DWORD error=GetLastError();if(error!=ERROR_IO_PENDING)Fail(error);
        HANDLE waits[2]{done.h,stop};DWORD wait=WaitForMultipleObjects(stop?2u:1u,waits,FALSE,timeout);
        if(wait!=WAIT_OBJECT_0){
            DWORD waitError=wait==WAIT_FAILED?GetLastError():ERROR_TIMEOUT;
            CancelIoEx(device_,&op);
            if(!GetOverlappedResult(device_,&op,&got,TRUE)){
                error=GetLastError();if(error==ERROR_OPERATION_ABORTED&&(wait==WAIT_TIMEOUT||wait==WAIT_OBJECT_0+1))return {};Fail(error==ERROR_OPERATION_ABORTED?waitError:error);
            }
            // Completion may win the timeout/cancellation race. Return its real
            // batch instead of silently consuming and discarding those events.
        }else if(!GetOverlappedResult(device_,&op,&got,FALSE)){error=GetLastError();if(error==ERROR_OPERATION_ABORTED)return {};Fail(error);}
    }
    if(got!=sizeof(out)||!Valid(out))Fail(ERROR_INVALID_DATA);return out;
}
