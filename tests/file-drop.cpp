// SPDX-License-Identifier: MIT
#include "FileDrop.h"
#include "FileClipboard.h"
#include <Application.h>
#include <Node.h>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
int checks=0;
void Check(bool ok,const char *s){printf("%s %s\n",ok?"PASS":"FAIL",s);if(!ok)throw std::runtime_error(s);++checks;}
void Write(const std::string &p,const char *v){std::ofstream(p)<<v;}
std::string Read(const std::string &p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
bool Exists(const std::string &p){struct stat st;return lstat(p.c_str(),&st)==0;}
template<class F>bool Reject(F f){try{f();return false;}catch(...){return true;}}
int main(){BApplication app("application/x-vnd.Kambuzi-DropTests");char tmp[]="/boot/home/listi-drop-XXXXXX";if(!mkdtemp(tmp))return 2;std::string root(tmp),a=root+"/a",b=root+"/b";mkdir(a.c_str(),0755);mkdir(b.c_str(),0755);
try{
 auto f=a+"/æ $' file.txt";Write(f,"original");Write(a+"/two","second");
 auto drag=MakeFileDrag({f,a+"/two"});Check(drag.what==B_SIMPLE_DATA && drag.HasRef("refs"),"native drag uses file refs");
 auto job=PrepareFileDrop(drag,b);Check(job.sources.size()==2 && job.expectedSources.size()==2,"multi-selection identities frozen");
 Check(!Exists(b+"/two") && Exists(f),"preparing drop does not mutate files");
 auto result=Run(job,{});Check(result.completed==2 && Read(b+"/two")=="second" && Read(f)=="original","explicit copy handles Unicode and shell punctuation");
 Check(Reject([&]{PrepareFileDrop(drag,a);}),"same-folder drop rejected");
 BMessage external(drag);external.RemoveName("listi:drag-selection");Check(PrepareFileDrop(external,b).sources.size()==2,"external Tracker-style refs accepted");
 BMessage malformed(drag);entry_ref ref;drag.FindRef("refs",&ref);malformed.AddRef("refs",&ref);Check(Reject([&]{PrepareFileDrop(malformed,b);}),"duplicate refs rejected");
 Check(Reject([&]{PrepareFileDrop(BMessage(B_SIMPLE_DATA),b);}),"empty drop rejected");
 Check(Reject([&]{PrepareFileDrop(drag,"@disks");}),"volume listing rejected as destination");
 Check(Reject([&]{MakeFileDrag({"/boot"});}),"volume root refused");
 mkdir((a+"/folder").c_str(),0755);mkdir((a+"/folder/sub").c_str(),0755);Write(a+"/folder/sub/data","nested");
 Check(Reject([&]{MakeFileDrag({a+"/folder",a+"/folder/sub/data"});}),"overlapping selections rejected");
 auto folder=MakeFileDrag({a+"/folder"});Check(Reject([&]{PrepareFileDrop(folder,a+"/folder/sub");}),"folder into descendant rejected");
 symlink((a+"/folder/sub").c_str(),(root+"/alias").c_str());Check(Reject([&]{PrepareFileDrop(folder,root+"/alias");}),"destination alias into source rejected");
 job=PrepareFileDrop(MakeFileDrag({a+"/two"}),b);job.action=Action::Move;
 Hooks skip;skip.conflict=[](const auto&){return Decision::Skip;};result=Run(job,skip);Check(result.skipped==1 && Exists(a+"/two"),"skip preserves original move source");
 Hooks abort;abort.conflict=[](const auto&){return Decision::Abort;};result=Run(job,abort);Check(result.cancelled && Exists(a+"/two"),"cancel preserves original move source");
 Hooks replace;replace.conflict=[](const auto&){return Decision::Replace;};result=Run(job,replace);Check(result.completed==1 && !Exists(a+"/two") && Read(b+"/two")=="second","explicit move commits then removes original");
 job=PrepareFileDrop(MakeFileDrag({f}),b);rename(f.c_str(),(a+"/old").c_str());Write(f,"replacement");
 Check(Reject([&]{PrepareFileDrop(drag,b);}),"source replaced during drag rejected");
 result=Run(job,replace);Check(result.completed==0 && !result.errors.empty() && Read(f)=="replacement","source replaced during question rejected");
 job=PrepareFileDrop(MakeFileDrag({f}),b);rename(b.c_str(),(root+"/old-b").c_str());mkdir(b.c_str(),0755);result=Run(job,{});Check(result.completed==0 && !Exists(b+"/æ $' file.txt"),"destination replaced during question rejected");
 job=PrepareFileDrop(MakeFileDrag({f}),b);Write(b+"/æ $' file.txt","target");Hooks mutate;mutate.conflict=[&](const auto&){rename(f.c_str(),(a+"/old2").c_str());Write(f,"third");return Decision::Replace;};result=Run(job,mutate);Check(result.completed==0 && Read(b+"/æ $' file.txt")=="target" && Read(f)=="third","conflict-time source replacement leaves target untouched");
 symlink("missing-target",(a+"/link").c_str());job=PrepareFileDrop(MakeFileDrag({a+"/link"}),b);result=Run(job,{});struct stat st;Check(result.completed==1 && lstat((b+"/link").c_str(),&st)==0 && S_ISLNK(st.st_mode),"broken symlink copied without following it");
 auto remote=MakeFileDrag({"sftp://example.invalid/file"});job=PrepareFileDrop(remote,b);Check(job.sources.size()==1 && job.expectedSources.empty() && job.action==Action::Copy,"virtual provider drop remains copy-only without network access");
 job=PrepareFileDrop(MakeFileDrag({f}),"sftp://example.invalid/folder");Check(job.action==Action::Copy && job.expectedSources.count(f)==1,"local to virtual copy captures source identity");
 job.action=Action::Move;result=Run(job,{});Check(!result.errors.empty()&&!result.completed&&Exists(f),"local to virtual move refuses removal of original");
 Check(Reject([&]{MakeFileDrag({f,"sftp://example.invalid/file"});}),"mixed provider drag rejected");
 printf("%d drop checks passed\n",checks);return 0;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
