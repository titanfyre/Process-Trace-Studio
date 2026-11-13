#include "SDK/ProcessTrace.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<ProcessTrace::Client> session;
ProcessTrace::Client& Get(){if(!session)session=std::make_unique<ProcessTrace::Client>();return *session;}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto value=FromWire<PT_STATS>(a.at("status"));return {{"valid",ProcessTrace::Valid(value)},{"value",EncodeWire(value)}};}
if(op=="start"){Get().Start(a.value("images",true),a.value("full_paths",false));return Json::object();}
if(op=="stop"){if(session)session->Stop();return Json::object();}
if(op=="cancel"){if(session)session->Cancel();return Json::object();}
if(op=="status")return EncodeWire(Get().Status());
if(op=="receive")return EncodeWire(Get().Receive(Number(a,"timeout",0,100)));
if(op=="format_batch"){auto batch=FromWire<PT_BATCH>(a.at("batch"));Require(ProcessTrace::Valid(batch),"Invalid process trace batch");return {{"csv",ProcessTrace::Csv(batch)}};}
if(op=="event_name"){auto e=FromWire<PT_EVENT>(a.at("event"));Require(e.name_chars<PT_NAME_CHARS,"Name exceeds capacity");return {{"name",Utf8(ProcessTrace::Name(e))}};}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned process_trace_studio_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned process_trace_studio_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* process_trace_studio_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void process_trace_studio_json_free(char* response)noexcept{delete[] response;}
