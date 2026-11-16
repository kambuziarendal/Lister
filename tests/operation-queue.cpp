// SPDX-License-Identifier: MIT
#include "OperationQueue.h"
#include "JobSnapshot.h"
#include <Application.h>
#include <Window.h>
#include <StringView.h>
#include <LayoutBuilder.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
std::atomic<int> failedWakes{0};
extern "C" status_t __real__ZNK10BMessenger11SendMessageEP8BMessageP8BHandlerl(const BMessenger*,BMessage*,BHandler*,bigtime_t);
extern "C" status_t __wrap__ZNK10BMessenger11SendMessageEP8BMessageP8BHandlerl(const BMessenger* self,BMessage* m,BHandler* reply,bigtime_t timeout){
 if(m->what==kQueueWake&&failedWakes.load()>0){--failedWakes;return B_TIMED_OUT;}
 return __real__ZNK10BMessenger11SendMessageEP8BMessageP8BHandlerl(self,m,reply,timeout);
}
class QueueWindow:public BWindow {
public:
 OperationQueue *queue=nullptr;bool hold=true;std::vector<BMessage> completed;
 std::vector<int64> conflicts;
 QueueWindow():BWindow(BRect(10,10,910,300),"Queue regression",B_TITLED_WINDOW,0){
  queue=new OperationQueue(BMessenger(this));BLayoutBuilder::Group<>(this,B_VERTICAL).Add(queue->View());queue->Start();
 }
 ~QueueWindow(){delete queue;}
 void MessageReceived(BMessage*m) override {
  if(m->what==kQueueWake){
   if(hold)return;
   for(auto &event:queue->TakeEvents()){
    queue->PresentEvent(event);
    if(event.what==kJobDone)completed.push_back(event);
    if(event.what==kConflict)conflicts.push_back(event.GetInt64("request",-1));
   }
  }else BWindow::MessageReceived(m);
 }
};
void clean(const std::string&p){BDirectory d(p.c_str());BEntry e;while(d.GetNextEntry(&e,false)==B_OK){struct stat s;e.GetStat(&s);if(S_ISDIR(s.st_mode)){BPath path;e.GetPath(&path);clean(path.Path());}e.Remove();}rmdir(p.c_str());}
std::string bytes(const std::string&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 BApplication app("application/x-vnd.Listi-QueueRegression");char temp[]="/tmp/Listi-queue-XXXXXX";if(!mkdtemp(temp))return 2;std::string root=temp;
 mkdir((root+"/in").c_str(),0700);mkdir((root+"/out").c_str(),0700);
 int checks=0,failures=0;auto check=[&](bool ok,const char*label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;};
 auto window=new QueueWindow;window->Show();
 auto wait=[&](const std::function<bool()> &f){for(int i=0;i<400;++i){window->Lock();bool ok=f();window->Unlock();if(ok)return true;snooze(25000);}return false;};
 auto job=[&](int n,int64 tag){auto p=root+"/in/item"+std::to_string(n);Job j{Action::Copy,{p},root+"/out"};FreezeLocalJob(j,j.destination);j.tag=tag;return j;};
 failedWakes=100;
 for(int i=0;i<20;++i){std::ofstream(root+"/in/item"+std::to_string(i))<<std::string(32768,char('A'+i));window->queue->Enqueue(job(i,i+1));}
 check(wait([&]{return !window->queue->Busy();}),"20 jobs finish despite failed wakeups and held UI consumption");
 window->Lock();check(window->completed.empty(),"held UI has not consumed terminal state");window->hold=false;window->Unlock();
 check(wait([&]{return window->completed.size()==20;}),"periodic delivery recovers all retained completions");
 window->Lock();std::set<int64> tags;bool successful=true;for(auto&m:window->completed){tags.insert(m.GetInt64("tag",0));successful&=m.GetBool("complete",false);}check(tags.size()==20&&successful,"each successful job completes exactly once with its own tag");window->Unlock();
 bool intact=true;for(int i=0;i<20;++i)intact&=bytes(root+"/out/item"+std::to_string(i))==std::string(32768,char('A'+i));check(intact,"all 20 destinations contain exact payloads");
 window->queue->Enqueue(job(0,101));check(wait([&]{return window->conflicts.size()==1;}),"conflict reaches UI after wakeup failure");
 window->Lock();auto old=window->conflicts.back();window->Unlock();
 window->queue->Pause();
 for(int i=1;i<5;++i)window->queue->Enqueue(job(i,101+i));
 check(window->queue->Answer(Decision::Replace,old),"active conflict accepts its own answer");
 check(!window->queue->Answer(Decision::Replace,old),"duplicate conflict answer cannot execute twice");
 window->queue->Cancel();
 check(wait([&]{return !window->queue->Busy()&&window->completed.size()==25;}),"Cancel releases active paused job and all four queued tags");
 window->queue->Enqueue(job(0,201));check(wait([&]{return window->conflicts.size()==2;}),"fresh conflict works after cancellation");
 check(!window->queue->Answer(Decision::Replace,old),"old answer cannot resolve new conflict");
 window->Lock();auto current=window->conflicts.back();window->Unlock();check(window->queue->Answer(Decision::Skip,current),"new conflict accepts Skip");
 check(wait([&]{return !window->queue->Busy()&&window->completed.size()==26;}),"fresh skipped job reaches terminal state");
 // Delayed old progress must not revive a completed operation's controls.
 window->Lock();BMessage stale(kJobUpdate);stale.AddInt64("operation",1);stale.AddString("title","STALE JOB");window->queue->PresentEvent(stale);
 auto title=dynamic_cast<BStringView*>(window->FindView("operation"));check(title&&std::string(title->Text())!="STALE JOB","late progress cannot revive an old job");window->Unlock();
 window->queue->Enqueue(job(0,301));check(wait([&]{return window->conflicts.size()==3;}),"shutdown fixture is genuinely waiting at conflict");
 auto start=system_time();window->Lock();window->Quit();check(system_time()-start<2000000,"queue destruction releases blocked conflict without UI-thread deadlock");
 clean(root);std::cout<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
