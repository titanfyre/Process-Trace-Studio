#include "../SDK/ProcessTrace.h"
#include "Sample.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
namespace { HANDLE stopEvent;BOOL WINAPI Ctrl(DWORD){SetEvent(stopEvent);return TRUE;} }
int wmain(int argc,wchar_t** argv){
    try{
        bool preview=false,images=true,paths=false;DWORD seconds=10;
        for(int i=1;i<argc;++i){std::wstring a=argv[i];if(a==L"--preview")preview=true;else if(a==L"--no-images")images=false;else if(a==L"--full-paths")paths=true;else if(a==L"--seconds"&&i+1<argc){std::wstring v=argv[++i];size_t n=0;auto value=std::stoul(v,&n);if(n!=v.size()||value<1||value>3600)throw std::invalid_argument("Seconds must be 1..3600.");seconds=value;}else{std::wcerr<<L"Usage: ProcessTraceCLI [--preview] [--seconds 1..3600] [--no-images] [--full-paths]\n";return 2;}}
        std::cout<<"session,sequence,elapsed_ms,type,pid,process_key,create_time_100ns,parent_pid,parent_key,exit_status,flags,image\r\n";
        if(preview){std::cout<<ProcessTrace::Csv(Sample());return 0;}
        std::cerr<<"Read-only process telemetry. Explicit capture; Ctrl+C stops. No command lines or environment are recorded.\n";
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)throw std::runtime_error("Could not create cancellation event.");SetConsoleCtrlHandler(Ctrl,TRUE);
        ProcessTrace::Client client;client.Start(images,paths);ULONGLONG end=GetTickCount64()+static_cast<ULONGLONG>(seconds)*1000;
        while(WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0){ULONGLONG now=GetTickCount64();if(now>=end)break;DWORD left=static_cast<DWORD>(end-now);auto batch=client.Receive((std::min)(left,1000ul),stopEvent);if(batch.stats.version)std::cout<<ProcessTrace::Csv(batch);}
        auto stats=client.Status();client.Stop();std::cerr<<"Observed: "<<stats.received<<"; dropped: "<<stats.dropped<<"; truncated names: "<<stats.truncated<<".\n";
        SetConsoleCtrlHandler(Ctrl,FALSE);CloseHandle(stopEvent);return 0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<'\n';if(stopEvent){SetConsoleCtrlHandler(Ctrl,FALSE);CloseHandle(stopEvent);}return 1;}
}
