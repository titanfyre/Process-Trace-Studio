#include "../SDK/ProcessTrace.h"
#include "../App/Sample.h"
#include <iostream>
#include <random>
#include <deque>
#include <array>
#include <memory>
namespace {size_t checks;void Check(bool ok){++checks;if(!ok)throw std::runtime_error("Protocol validation failed at check "+std::to_string(checks));}}
int main(){try{
    PT_REQUEST request{};request.version=PT_VERSION;request.size=sizeof(request);
    Check(PT_ValidRequest(&request,PT_START)!=0);
    for(auto code:{PT_STOP,PT_STATUS,PT_RECEIVE})Check(PT_ValidRequest(&request,code)!=0);
    for(unsigned i=0;i<32;++i){auto r=request;r.flags=1ul<<i;Check(PT_ValidRequest(&r,PT_START)==(i<2));Check(!PT_ValidRequest(&r,PT_STATUS));}
    for(auto& n:request.reserved){n=1;Check(!PT_ValidRequest(&request,PT_START));n=0;}
    request.version=2;Check(!PT_ValidRequest(&request,PT_START));request.version=1;request.size=31;Check(!PT_ValidRequest(&request,PT_START));Check(!PT_ValidRequest(&request,0));
    PT_EVENT name{};const PT_U16 path[]{'C',':','\\','u','s','e','r','\\','a','.','e','x','e'};
    PT_CopyName(&name,path,13,0);Check(ProcessTrace::Name(name)==L"a.exe");name={};PT_CopyName(&name,path,13,PT_FULL_PATHS);Check(ProcessTrace::Name(name)==L"C:\\user\\a.exe");
    std::array<PT_U16,600> longName{};longName.fill('x');name={};PT_CopyName(&name,longName.data(),600,0);Check(name.name_chars==259&&(name.flags&PT_FLAG_TRUNCATED));
    longName[258]=0xD800;name={};PT_CopyName(&name,longName.data(),600,0);Check(name.name_chars==258&&(name.flags&PT_FLAG_TRUNCATED));
    auto sample=Sample();Check(ProcessTrace::Valid(sample));Check(sample.events[0].pid==sample.events[3].pid);Check(sample.events[0].process_key!=sample.events[3].process_key);Check(sample.events[0].process_key==sample.events[2].process_key);
    auto bad=sample;bad.count=PT_BATCH_MAX+1;Check(!ProcessTrace::Valid(bad));bad=sample;bad.events[0].name_chars=PT_NAME_CHARS;Check(!ProcessTrace::Valid(bad));bad=sample;bad.events[1].sequence=bad.events[0].sequence;Check(!ProcessTrace::Valid(bad));bad=sample;bad.stats.reserved[0]=1;Check(!ProcessTrace::Valid(bad));bad=sample;bad.events[0].process_key=0;Check(!ProcessTrace::Valid(bad));
    bad=sample;bad.events[0].name[0]='=';auto csv=ProcessTrace::Csv(bad);Check(csv.find("\"'=xampleApp.exe\"")!=std::string::npos);
    auto storage=std::make_unique<PT_EVENT[]>(PT_RING_CAPACITY);PT_U32 head=0,count=0;PT_STATS stats=sample.stats;stats.received=stats.dropped=stats.truncated=0;std::deque<PT_EVENT> reference;std::mt19937 random(0x54495441);PT_U64 last=0,overwrites=0;
    for(unsigned i=0;i<100000;++i){
        if(random()%5!=0){PT_EVENT e=sample.events[i%5];if(random()%13==0)e.flags|=PT_FLAG_TRUNCATED;PT_RingPush(storage.get(),&head,&count,&stats,&e);if(reference.size()==PT_RING_CAPACITY){reference.pop_front();++overwrites;}reference.push_back(e);}
        else{PT_BATCH out{};PT_RingDrain(storage.get(),&head,&count,&stats,&out);Check(ProcessTrace::Valid(out));for(PT_U32 n=0;n<out.count;++n){Check(!reference.empty());Check(out.events[n].sequence==reference.front().sequence);Check(out.events[n].sequence>last);last=out.events[n].sequence;reference.pop_front();}}
        Check(count==reference.size());Check(stats.dropped==overwrites);Check(count<=PT_RING_CAPACITY);
    }
    // Actual kernel capture-commit primitive: close, stale owner and PnP removal
    // must leave a stopped session unchanged, in every ordering combination.
    for(int owner=0;owner<2;++owner)for(int closing=0;closing<2;++closing)for(int removing=0;removing<2;++removing){auto s=sample.stats;s.active=0;auto old=s;bool committed=PT_CommitCapture(&s,owner,closing,removing,0,10000000,10000000,123)!=0;Check(committed==(owner&&!closing&&!removing));if(!committed)Check(memcmp(&s,&old,sizeof(s))==0);else Check(s.active==1&&s.session==old.session+1&&s.received==0&&s.queued==0);}
    // Bounded random wire fields are rejected safely before name/data reads.
    for(unsigned i=0;i<20000;++i){auto b=Sample();switch(random()%5){case 0:b.count=random();break;case 1:b.events[0].name_chars=random();break;case 2:b.events[0].flags=random();break;case 3:b.stats.version=random();break;default:b.stats.queued=random();break;}bool valid=ProcessTrace::Valid(b);Check(!valid);}
    std::cout<<"Passed "<<checks<<" protocol, ring, overflow, lifecycle, PID reuse, privacy and CSV checks. No driver was installed or opened.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
