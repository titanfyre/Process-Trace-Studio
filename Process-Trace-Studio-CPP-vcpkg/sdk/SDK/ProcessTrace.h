#pragma once
#include <string.h>
/* Stable pointer-free wire format shared with the private driver. */
typedef unsigned long PT_U32;
typedef unsigned __int64 PT_U64;
typedef unsigned short PT_U16;
#define PT_VERSION 1u
#define PT_NAME_CHARS 260u
#define PT_BATCH_MAX 16u
#define PT_RING_CAPACITY 512u
#define PT_CAPTURE_IMAGES 1u
#define PT_FULL_PATHS 2u
#define PT_EVENT_CREATE 1u
#define PT_EVENT_EXIT 2u
#define PT_EVENT_IMAGE 3u
#define PT_FLAG_TRUNCATED 1u
#define PT_FLAG_KERNEL_IMAGE 2u
#define PT_FLAG_IDENTITY_UNKNOWN 4u
#define PT_FLAG_PARENT_OBSERVED 8u
#define PT_IOCTL(n) ((0x8000u<<16)|(3u<<14)|((n)<<2))
#define PT_START PT_IOCTL(0x800u)
#define PT_STOP PT_IOCTL(0x801u)
#define PT_STATUS PT_IOCTL(0x802u)
#define PT_RECEIVE PT_IOCTL(0x803u)
typedef struct PT_REQUEST {
    PT_U32 version, size, flags, reserved[5];
} PT_REQUEST;
typedef struct PT_EVENT {
    PT_U64 sequence, qpc, process_key, create_time_100ns, parent_key;
    PT_U32 type, pid, parent_pid, exit_status, flags, name_chars;
    PT_U16 name[PT_NAME_CHARS];
} PT_EVENT;
typedef struct PT_STATS {
    PT_U32 version, size, active, options;
    PT_U64 session, frequency, start_qpc, start_utc_100ns;
    PT_U64 received, dropped, truncated;
    PT_U32 queued, capacity, reserved[2];
} PT_STATS;
typedef struct PT_BATCH {
    PT_STATS stats;
    PT_U32 count, reserved[3];
    PT_EVENT events[PT_BATCH_MAX];
} PT_BATCH;

/* Small, deterministic protocol primitives used by the kernel and its offline
   tests. Callers provide synchronization and valid, bounded memory. */
static __inline int PT_ValidRequest(const PT_REQUEST* r,PT_U32 code) {
    PT_U32 i;
    if(code!=PT_START&&code!=PT_STOP&&code!=PT_STATUS&&code!=PT_RECEIVE)return 0;
    if(r->version!=PT_VERSION||r->size!=sizeof(*r)||(r->flags&~(PT_CAPTURE_IMAGES|PT_FULL_PATHS))||(code!=PT_START&&r->flags))return 0;
    for(i=0;i<5;++i)if(r->reserved[i])return 0;
    return 1;
}
static __inline int PT_CommitCapture(PT_STATS* s,int ownerMatches,int fileClosing,int removing,PT_U32 flags,PT_U64 frequency,PT_U64 qpc,PT_U64 utc) {
    if(!ownerMatches||fileClosing||removing)return 0;
    ++s->session;s->active=1;s->options=flags;s->frequency=frequency;s->start_qpc=qpc;s->start_utc_100ns=utc;s->received=s->dropped=s->truncated=0;s->queued=0;return 1;
}
static __inline void PT_CopyName(PT_EVENT* e,const PT_U16* text,PT_U32 chars,PT_U32 options) {
    PT_U32 start=0,i;
    if(!text)return;
    if(!(options&PT_FULL_PATHS))for(i=chars;i>0;--i)if(text[i-1]=='\\'||text[i-1]=='/'){start=i;break;}
    chars-=start;
    if(chars>=PT_NAME_CHARS){chars=PT_NAME_CHARS-1;e->flags|=PT_FLAG_TRUNCATED;}
    // Do not cut a UTF-16 surrogate pair at the fixed buffer boundary.
    if(chars&&text[start+chars-1]>=0xD800&&text[start+chars-1]<=0xDBFF){--chars;e->flags|=PT_FLAG_TRUNCATED;}
    for(i=0;i<chars;++i)e->name[i]=text[start+i];e->name[chars]=0;e->name_chars=chars;
}
static __inline void PT_RingPush(PT_EVENT* storage,PT_U32* head,PT_U32* count,PT_STATS* stats,PT_EVENT* event) {
    event->sequence=++stats->received;
    if(event->flags&PT_FLAG_TRUNCATED)++stats->truncated;
    if(*count==PT_RING_CAPACITY){*head=(*head+1)%PT_RING_CAPACITY;--*count;++stats->dropped;}
    storage[(*head+*count)%PT_RING_CAPACITY]=*event;++*count;
}
static __inline void PT_RingDrain(PT_EVENT* storage,PT_U32* head,PT_U32* count,const PT_STATS* stats,PT_BATCH* out) {
    PT_U32 i;memset(out,0,sizeof(*out));out->count=*count<PT_BATCH_MAX?*count:PT_BATCH_MAX;
    for(i=0;i<out->count;++i){out->events[i]=storage[*head];*head=(*head+1)%PT_RING_CAPACITY;}
    *count-=out->count;out->stats=*stats;out->stats.queued=*count;
}

#ifndef _KERNEL_MODE
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <stdexcept>
static_assert(sizeof(PT_REQUEST)==32 && sizeof(PT_EVENT)==584 && sizeof(PT_STATS)==88 && sizeof(PT_BATCH)==9448,"Unexpected protocol packing");
namespace ProcessTrace {
// Validation is also useful to reject malformed saved/synthetic batches.
bool Valid(const PT_STATS&) noexcept;
bool Valid(const PT_BATCH&) noexcept;
std::wstring Name(const PT_EVENT&);
std::string Csv(const PT_BATCH&);
std::wstring Error(unsigned long code);
class Client {
    HANDLE device_=INVALID_HANDLE_VALUE;
    mutable std::mutex control_;
    void Command(PT_U32 code, PT_U32 flags, void* output, DWORD bytes) const;
public:
    Client();
    ~Client();
    Client(const Client&)=delete;
    Client& operator=(const Client&)=delete;
    void Start(bool images=true, bool fullPaths=false);
    void Stop();
    PT_STATS Status() const;
    // Empty batch means timeout or stopEvent, never fabricated events.
    PT_BATCH Receive(DWORD timeout=INFINITE, HANDLE stopEvent=nullptr);
    void Cancel() noexcept;
};
}
#endif
