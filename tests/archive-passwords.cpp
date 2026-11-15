// SPDX-License-Identifier: MIT
#include "ArchivePasswords.h"
#include "ArchiveJobs.h"
#include "ArchiveBackend.h"
#include <Application.h>
#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <set>
#include <chrono>
using namespace Lister;
int checks=0;
void check(bool ok,const char *name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}++checks;printf("PASS %s\n",name);}
std::string read(const std::string &p){std::string out;ReadFile(p,[&](const char *b,size_t n){out.append(b,n);});return out;}
ArchiveSecretPtr secret(const char *s="Listi-test-only"){return std::make_shared<ArchiveSecret>(s);}
int main(){
 BApplication app("application/x-vnd.Listi-Password-Test");
 char cwd[4096];getcwd(cwd,sizeof(cwd));std::string fixtures=std::string(cwd)+"/tests/fixtures/passwords/";
 char temp[]="/boot/home/listi-password-test-XXXXXX";auto root=mkdtemp(temp);check(root,"test root");
 for(const char *method:{"ZipCrypto","AES128","AES256"}){
  ForgetArchivePasswords();int calls=0;
  SetArchivePasswordPrompt([&](const std::string &,bool,const TransferControl &){++calls;return secret();});
  std::string p=fixtures+method+".zip";
  auto archive=OpenArchive(p);auto entries=ListFiles(archive);check(entries.size()==2,"encrypted listing");
  check(read(entries[0].path)=="Private fixture alpha\n","decrypted alpha");
  check(read(entries[1].path)=="Private fixture beta\n","decrypted beta");
  check(calls==1,"one password for both entries");
  auto job=PrepareArchiveJob(true,{p});SetArchiveDestination(job,root,method);
  std::set<std::string> completed;
  Hooks hooks;hooks.fileDone=[&](const std::string &path,const std::string &){completed.insert(path);};
  auto result=RunArchiveJob(job,hooks);check(result.errors.empty() && result.completed==1,"encrypted atomic extraction");
  check(completed.count(entries[0].path) && completed.count(entries[1].path),"published members marked complete");
  check(calls==1,"extraction reuses in-memory password");
  ForgetArchivePasswords();read(entries[0].path);check(calls==2,"forget requires new prompt");
 }
 const std::string fixture=fixtures+"AES256.zip";
 auto entry=ListFiles(OpenArchive(fixture))[0].path;
 ForgetArchivePasswords();int calls=0;bool retrySeen=false;
 SetArchivePasswordPrompt([&](const std::string &,bool retry,const TransferControl &){retrySeen|=retry;return ++calls==1?secret("wrong"):secret();});
 check(read(entry)=="Private fixture alpha\n" && calls==2 && retrySeen,"wrong password then retry");
 ForgetArchivePasswords();calls=0;
 SetArchivePasswordPrompt([&](const std::string &,bool,const TransferControl &){++calls;return secret("wrong");});
 bool failed=false;try{read(entry);}catch(const std::exception &e){failed=std::string(e.what()).find("Password not accepted")!=std::string::npos;}
 check(failed && calls==4,"bounded wrong-password attempts");
 ForgetArchivePasswords();SetArchivePasswordPrompt([](const std::string &,bool,const TransferControl &){return ArchiveSecretPtr();});
 bool cancelled=false;try{read(entry);}catch(const Cancelled &){cancelled=true;}check(cancelled,"prompt cancel propagates");
 auto job=PrepareArchiveJob(true,{fixture});SetArchiveDestination(job,root,"cancelled");auto result=RunArchiveJob(job,{});
 check(result.cancelled && access((std::string(root)+"/cancelled").c_str(),F_OK)!=0,"cancel publishes nothing");
 ForgetArchivePasswords();SetArchivePasswordPrompt([](const std::string &,bool,const TransferControl &){ForgetArchivePasswords();return secret();});
 cancelled=false;try{read(entry);}catch(const Cancelled &){cancelled=true;}check(cancelled,"forget rejects late prompt answer");
 std::string changed=std::string(root)+"/changed.zip";
 {std::ifstream in(fixture,std::ios::binary);std::ofstream out(changed,std::ios::binary);out<<in.rdbuf();}
 auto changedEntry=ListFiles(OpenArchive(changed))[0].path;
 SetArchivePasswordPrompt([&](const std::string &,bool,const TransferControl &){rename(changed.c_str(),(changed+".old").c_str());std::ofstream(changed)<<"replacement";return secret();});
 failed=false;try{read(changedEntry);}catch(const std::exception &e){failed=std::string(e.what()).find("Archive changed")!=std::string::npos;}check(failed,"replacement during prompt rejected");
 SetArchivePasswordPrompt([](const std::string &,bool,const TransferControl &){return secret();});
 failed=false;try{auto archive=OpenArchive(fixtures+"encrypted.7z");ListFiles(archive);}catch(const std::exception &e){failed=std::string(e.what()).find("not supported")!=std::string::npos;}check(SevenZipAvailable()?!failed:failed,"encrypted 7z uses optional engine or rejects explicitly");
 struct stat st{};stat(fixture.c_str(),&st);auto identity=ArchivePasswordIdentity(st);auto generation=ArchivePasswordGeneration();
 auto another=st;another.st_ino++;check(identity!=ArchivePasswordIdentity(another),"cache identity binds inode");
 another=st;another.st_mtim.tv_nsec++;check(identity!=ArchivePasswordIdentity(another),"cache identity binds modification");
 another=st;another.st_ctim.tv_nsec++;check(identity!=ArchivePasswordIdentity(another),"cache identity binds metadata");
 ForgetArchivePasswords();generation=ArchivePasswordGeneration();calls=0;
 SetArchivePasswordPrompt([&](const std::string &,bool,const TransferControl &){++calls;return secret();});
 for(int i=0;i<17;++i)RememberArchivePassword("key"+std::to_string(i),secret(),generation);
 GetArchivePassword(fixture,"key16",false,generation,{});check(calls==0,"newest cache entry retained");
 GetArchivePassword(fixture,"key0",false,generation,{});check(calls==1,"cache bounded to sixteen archives");
 auto creation=HoldArchiveCreationPassword(secret("creation-only"));
 check(creation && std::string(TakeArchiveCreationPassword(creation)->Text())=="creation-only","creation password transferred once");
 cancelled=false;try{TakeArchiveCreationPassword(creation);}catch(const Cancelled &){cancelled=true;}
 check(cancelled,"creation password token is single use");
 creation=HoldArchiveCreationPassword(secret("forget-me"));ForgetArchivePasswords();
 cancelled=false;try{TakeArchiveCreationPassword(creation);}catch(const Cancelled &){cancelled=true;}
 check(cancelled,"forget clears pending creation passwords");
 std::vector<uint64_t> pending;
 for(int i=0;i<16;++i)pending.push_back(HoldArchiveCreationPassword(secret(("pending-"+std::to_string(i)).c_str())));
 bool bounded=false;try{HoldArchiveCreationPassword(secret("seventeen"));}catch(const std::exception &){bounded=true;}
 check(bounded && std::string(TakeArchiveCreationPassword(pending.front())->Text())=="pending-0",
   "creation registry refuses overflow without evicting queued password");
 for(size_t i=1;i<pending.size();++i)DiscardArchiveCreationPassword(pending[i]);
 generation=ArchivePasswordGeneration();
 // A request waiting behind another prompt must still observe cancellation.
 std::atomic<bool> entered{false},release{false};
 SetArchivePasswordPrompt([&](const std::string &,bool,const TransferControl &c){entered=true;while(!release){c.Check();std::this_thread::sleep_for(std::chrono::milliseconds(10));}return secret();});
 std::thread first([&]{GetArchivePassword(fixture,"first",false,generation,{});});
 while(!entered)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 TransferControl control;int polls=0;control.proceed=[&]{return ++polls<3;};
 cancelled=false;try{GetArchivePassword(fixture,"second",false,generation,control);}catch(const Cancelled &){cancelled=true;}
 release=true;first.join();check(cancelled,"queued prompt observes cancellation");
 SetArchivePasswordPrompt([](const std::string &,bool,const TransferControl &){return secret();});
 StopArchivePasswords();cancelled=false;try{GetArchivePassword(fixture,identity,false,generation,{});}catch(const Cancelled &){cancelled=true;}check(cancelled,"shutdown cancels pending password requests");
 printf("%d archive password checks passed\n",checks);
}
