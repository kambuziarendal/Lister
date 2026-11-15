#include "DirectoryScanner.h"
#include "Common.h"
#include <Application.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
class Receiver:public BLooper {
public:
 sem_id entered=create_sem(0,"probe entered"),release=create_sem(0,"probe release"),done=create_sem(0,"probe done");
 int count=0;std::string error;
 Receiver():BLooper("bounded receiver",B_NORMAL_PRIORITY,1){}
 ~Receiver(){delete_sem(entered);delete_sem(release);delete_sem(done);}
 void MessageReceived(BMessage *m) override {
  if(m->what=='hold'){release_sem(entered);acquire_sem(release);}
  else if(m->what==kRows){BMessage row;for(int32 i=0;m->FindMessage("entry",i,&row)==B_OK;++i)if(!row.GetBool("parent",false))++count;}
  else if(m->what==kScanDone){error=m->GetString("error","");release_sem(done);}
  else BLooper::MessageReceived(m);
 }
};
int main(){
 BApplication app("application/x-vnd.Listi-AuditDeliveryProbe");
 char path[]="/boot/home/Listi-delivery-probe-XXXXXX";if(!mkdtemp(path))return 2;
 for(int i=0;i<160;++i)std::ofstream(std::string(path)+"/file-"+std::to_string(i))<<"probe";
 auto receiver=new Receiver;receiver->Run();auto gen=std::make_shared<std::atomic<int64>>(1);
 auto scan=CreateDirectoryScanner(BMessenger(receiver),gen);scan->Run();
 auto request=[&](int64 n){BMessage m(kScan);m.AddInt64("serial",n);m.AddString("path",path);m.AddBool("icons",false);scan->PostMessage(&m);};
 request(1);if(acquire_sem_etc(receiver->done,1,B_RELATIVE_TIMEOUT,10000000)!=B_OK)return 3;
 receiver->Lock();int normal=receiver->count;std::string normalError=receiver->error;receiver->count=0;receiver->error.clear();receiver->Unlock();
 std::cout<<"normal: expected=160 received="<<normal<<" error="<<normalError<<std::endl;
 BMessenger target(receiver);BMessage hold('hold'),fill('fill');target.SendMessage(&hold);
 if(acquire_sem_etc(receiver->entered,1,B_RELATIVE_TIMEOUT,3000000)!=B_OK)return 4;
 // The handler is blocked and its one-message port is occupied before scanning.
 if(target.SendMessage(&fill,(BHandler*)nullptr,100000)!=B_OK)return 5;
 gen->store(2);request(2);snooze(750000);release_sem(receiver->release);
 if(acquire_sem_etc(receiver->done,1,B_RELATIVE_TIMEOUT,10000000)!=B_OK)return 6;
 receiver->Lock();int blocked=receiver->count;std::string error=receiver->error;receiver->Unlock();
 std::cout<<"backpressure: expected=160 received="<<blocked<<" error="<<error<<std::endl;
 bool reproduced=normal==160&&normalError.empty()&&blocked<160&&error.empty();
 std::cout<<(reproduced?"REPRODUCED silent dropped directory rows followed by successful completion":"NOT REPRODUCED")<<std::endl;
 gen->fetch_add(1);if(scan->Lock())scan->Quit();if(receiver->Lock())receiver->Quit();
 for(int i=0;i<160;++i)unlink((std::string(path)+"/file-"+std::to_string(i)).c_str());rmdir(path);
 return reproduced?0:1;
}
