#pragma once
#include "../SDK/ProcessTrace.h"
inline PT_BATCH Sample(){
    PT_BATCH b{};b.stats.version=PT_VERSION;b.stats.size=sizeof(b.stats);b.stats.capacity=PT_RING_CAPACITY;b.stats.session=1;b.stats.frequency=10000000;b.stats.start_qpc=10000000;b.stats.received=5;b.count=5;
    const wchar_t* names[]{L"ExampleApp.exe",L"ExampleRuntime.dll",L"",L"ExampleWorker.exe",L"ExampleModule.dll"};
    for(PT_U32 i=0;i<b.count;++i){auto& e=b.events[i];e.sequence=i+1;e.qpc=b.stats.start_qpc+1000000*i;e.type=i==2?PT_EVENT_EXIT:(i==1||i==4?PT_EVENT_IMAGE:PT_EVENT_CREATE);e.pid=4200;e.process_key=i<3?0x13570:0x13571;e.create_time_100ns=i<3?100000:200000;if(e.type==PT_EVENT_CREATE){e.parent_pid=3100;e.parent_key=0x12001;}
        if(e.type==PT_EVENT_CREATE)e.flags|=PT_FLAG_PARENT_OBSERVED;
        e.name_chars=static_cast<PT_U32>(wcslen(names[i]));for(PT_U32 j=0;j<e.name_chars;++j)e.name[j]=static_cast<PT_U16>(names[i][j]);
    }return b;
}
