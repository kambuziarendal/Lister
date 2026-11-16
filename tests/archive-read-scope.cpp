// SPDX-License-Identifier: MIT
#include "ArchiveBackend.h"
#include "ArchiveJobs.h"
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
std::string content(const std::string&p,const TransferControl& c={}){std::string s;ReadFile(p,[&](const char*b,size_t n){s.append(b,n);},c);return s;}
void clean(const std::string&p){BDirectory d(p.c_str());BEntry e;while(d.GetNextEntry(&e,false)==B_OK){struct stat s;e.GetStat(&s);if(S_ISDIR(s.st_mode)){BPath path;e.GetPath(&path);clean(path.Path());}e.Remove();}rmdir(p.c_str());}
int main(){BApplication app("application/x-vnd.Listi-ArchiveReadScope");char temp[]="/tmp/Listi-read-scope-XXXXXX";if(!mkdtemp(temp))return 2;std::string root=temp;
 int checks=0,failures=0;auto check=[&](bool ok,const std::string&label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;};
 try{for(auto format:{"zip","tgz","cpio","xar","7z"}){
  std::ofstream(root+"/a")<<"aaaa";std::ofstream(root+"/b")<<"bbbb";
  auto create=PrepareArchiveJob(false,{root+"/b",root+"/a"});create.archiveFormat=format;SetArchiveDestination(create,root,std::string("archive-")+format);if(Run(create,{}).completed!=1)return 3;
  auto archive=OpenArchive(create.destination),a=ChildLocation(archive,"a"),b=ChildLocation(archive,"b");
  {
   ArchiveListingScope scope;
   FileEntry entry;check(StatFile(a,entry)&&entry.size==4,std::string(format)+" scope captures current listing");
   check(content(a)=="aaaa"&&content(b)=="bbbb"&&content(a)=="aaaa",std::string(format)+" out-of-order and repeated reads retain exact bytes");
   TransferControl cancelled;cancelled.proceed=[] {return false;};bool stopped=false;try{content(b,cancelled);}catch(const Cancelled&){stopped=true;}
   check(stopped&&content(b)=="bbbb",std::string(format)+" cancellation discards reader and next operation recovers");
   std::ofstream(root+"/a")<<"new content";auto edit=PrepareArchiveEditJob(create.destination,{root+"/a"},{});Hooks replace;replace.conflict=[](const std::string&){return Decision::Replace;};
   check(Run(edit,replace).completed==1&&StatFile(a,entry)&&entry.size==11&&content(a)=="new content",std::string(format)+" publication invalidates metadata and reader in same scope");
   std::ofstream(root+"/c")<<"cccc";edit=PrepareArchiveEditJob(create.destination,{root+"/c"},{});
   check(Run(edit,{}).completed==1&&ListFiles(archive).size()==3,std::string(format)+" added member invalidates cached directory set");
  }
  check(content(a)=="new content",std::string(format)+" reader is usable after scope cleanup");
 }
 // A cache must not keep encrypted names accessible after Forget passwords.
 std::ofstream(root+"/secret")<<"private fixture";
 auto create=PrepareArchiveJob(false,{root+"/secret"});create.archiveFormat="7z";create.archiveEncryptNames=true;create.archivePasswordToken=HoldArchiveCreationPassword(std::make_shared<ArchiveSecret>("fixture"));SetArchiveDestination(create,root,"encrypted");
 SetArchivePasswordPrompt([](const std::string&,bool,const TransferControl&){return std::make_shared<ArchiveSecret>("fixture");});
 check(Run(create,{}).completed==1,"encrypted scope fixture created");
 auto archive=OpenArchive(create.destination);
 {ArchiveListingScope scope;check(ListFiles(archive).size()==1,"encrypted directory is readable with password");ForgetArchivePasswords();SetArchivePasswordPrompt([](const std::string&,bool,const TransferControl&){return ArchiveSecretPtr{};});bool cancelled=false;try{ListFiles(archive);}catch(const Cancelled&){cancelled=true;}check(cancelled,"forgotten password cannot be bypassed by cached names");}
 SetArchivePasswordPrompt({});ForgetArchivePasswords();
 }catch(const std::exception&e){check(false,std::string("unexpected: ")+e.what());}
 clean(root);std::cout<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;}
