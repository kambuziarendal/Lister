// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include "ImagePreview.h"
#include "VirtualFile.h"
#include "MessageStore.h"
#include <Bitmap.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
int main(){
 char pattern[]="/tmp/Listi-audit-io-XXXXXX";char*root=mkdtemp(pattern);if(!root)return 2;
 std::string base=root,fifo=base+"/blocked.zip";if(mkfifo(fifo.c_str(),0600))return 2;
 int checks=0,failures=0;auto check=[&](bool ok,const char*s){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<s<<std::endl;};
 // Hard watchdog makes a blocking open a failed process, not a green timeout.
 alarm(15);
 bool rejected=false;try{Lister::OpenArchive(fifo);}catch(const std::exception&){rejected=true;}check(rejected,"archive FIFO refused without waiting for a writer");
 rejected=false;try{Lister::LoadMessageStore(fifo);}catch(const std::exception&){rejected=true;}check(rejected,"settings FIFO refused without waiting for a writer");
 rejected=false;try{Lister::ReadFile(fifo,[](const char*,size_t){});}catch(const std::exception&){rejected=true;}check(rejected,"text/special input refused without waiting");
 rejected=false;try{delete LoadPreviewBitmap(fifo,{});}catch(const std::exception&){rejected=true;}check(rejected,"image FIFO refused without waiting");
 std::string file=base+"/input";{std::ofstream out(file);out<<std::string(300000,'x');}
 struct Context{std::string path;bool passed=false;} ctx{file};
 pthread_attr_t attr;pthread_attr_init(&attr);pthread_attr_setstacksize(&attr,256*1024);
 pthread_t thread;auto worker=[](void*p)->void*{auto &ctx=*static_cast<Context*>(p);try{size_t n=0;Lister::ReadFile(ctx.path,[&](const char*,size_t size){n+=size;});ctx.passed=n==300000;}catch(...){}return nullptr;};
 if(pthread_create(&thread,&attr,worker,&ctx))return 2;pthread_attr_destroy(&attr);pthread_join(thread,nullptr);check(ctx.passed,"local read succeeds on 256 KiB worker stack");
 unlink(file.c_str());unlink(fifo.c_str());rmdir(base.c_str());alarm(0);
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
