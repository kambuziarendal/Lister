// SPDX-License-Identifier: MIT
#include "Pane.h"
#include "ImagePreview.h"
#include "SearchWindow.h"
#include "SavedSearches.h"
#include <Application.h>
#include <MessageFilter.h>
#include <Window.h>
#include <iostream>
#include <cstring>

// Hold a real owned reader inside its looper while the owner closes. This
// deterministically distinguishes joined shutdown from merely posting Quit.
struct Hold {
  sem_id entered = create_sem(0, "reader entered"), release = create_sem(0, "release reader");
  ~Hold() { delete_sem(entered); delete_sem(release); }
};
class Delay : public BMessageFilter {
  Hold &hold;
public:
  explicit Delay(Hold &h) : BMessageFilter('hold'), hold(h) {}
  filter_result Filter(BMessage *, BHandler **) override {
    release_sem(hold.entered);
    acquire_sem(hold.release);
    return B_SKIP_MESSAGE;
  }
};
// Thread names are system-wide on Haiku. Never hold or wait for a worker
// belonging to a running file manager or a second test process.
thread_id FindReader(const char *name) {
  int32 cookie = 0;
  thread_info info;
  while (get_next_thread_info(be_app->Team(), &cookie, &info) == B_OK) {
    if (strcmp(info.name, name) == 0)
      return info.thread;
  }
  return B_NAME_NOT_FOUND;
}

int main() {
  BApplication app("application/x-vnd.Listi-WorkerLifecycleTest");
  int checks=0, failures=0;
  auto check=[&](bool ok,const std::string &label) {
    ++checks; failures+=!ok; std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;
  };
  for (const char *name : {"thumbnail reader", "image preview reader", "directory reader", "folder tree reader", "file search"}) {
    auto window=new BWindow(BRect(10,10,810,610),"Worker lifecycle test",B_TITLED_WINDOW,0);
    window->AddChild(new Pane(0,Preferences{}));
    window->AddChild(new ImagePreview());
    window->Show();
    BMessenger search=OpenFileSearch(BMessenger(window),0,"/tmp",Listi::SavedSearches().Message());
    thread_id reader=FindReader(name);
    check(reader>=0,std::string(name)+" created by real view");
    if(reader<0)return 2;
    Hold hold;
    auto worker=BLooper::LooperForThread(reader);
    if(!worker || !worker->Lock())return 3;
    worker->AddCommonFilter(new Delay(hold));
    worker->PostMessage('hold');worker->Unlock();
    if(acquire_sem_etc(hold.entered,1,B_RELATIVE_TIMEOUT,10000000)!=B_OK)return 4;
    auto release=spawn_thread([](void *p)->int32 {snooze(250000);release_sem(static_cast<Hold*>(p)->release);return 0;},"reader test release",B_NORMAL_PRIORITY,&hold);
    resume_thread(release);
    bigtime_t start=system_time();
    if(!strcmp(name,"file search")) {
      BLooper *owner=nullptr;search.Target(&owner);
      if(!owner || !owner->Lock())return 5;
      owner->Quit();
    } else {window->Lock();window->Quit();window=nullptr;}
    thread_info info;
    check(get_thread_info(reader,&info)==B_BAD_THREAD_ID,std::string(name)+" has exited before owner destruction returns");
    check(system_time()-start<3000000,std::string(name)+" shutdown completes without UI deadlock");
    status_t result;wait_for_thread(release,&result);
    if(window){window->Lock();window->Quit();}
    if(strcmp(name,"file search")) {BLooper *owner=nullptr;search.Target(&owner);if(owner&&owner->Lock())owner->Quit();}
    // Also allow the unfixed negative control to finish its posted quits before
    // the next case or BApplication teardown; the assertion above stays failed.
    for(const char *other:{"thumbnail reader","image preview reader","directory reader","folder tree reader","file search"}) {
      thread_id id=FindReader(other);if(id>=0)wait_for_thread(id,&result);
    }
  }
  std::cout<<checks<<" checks, "<<failures<<" failures"<<std::endl;
  return failures?1:0;
}
