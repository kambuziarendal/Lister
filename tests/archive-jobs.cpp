// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "ArchivePasswords.h"
#include "ArchiveSettings.h"
#include "MessageStore.h"
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <Application.h>
#include <algorithm>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
using namespace Lister;
int passed=0;
void check(bool ok,const std::string &name){if(!ok)throw std::runtime_error("FAIL "+name);std::cout<<"PASS "<<++passed<<" "<<name<<std::endl;}
void write(const std::string &p,const std::string &s){std::ofstream(p)<<s;}
std::string read(const std::string &p){std::ifstream f(p);return std::string(std::istreambuf_iterator<char>(f),{});}
std::string vread(const std::string &p){std::string out;ReadFile(p,[&](const char *bytes,size_t size){out.append(bytes,size);});return out;}
bool exists(const std::string &p){struct stat s;return !lstat(p.c_str(),&s);}
ArchiveSecretPtr secret(const char *text="Listi-create-test-Æ"){return std::make_shared<ArchiveSecret>(text);}
void clean(const std::string &p){BDirectory d(p.c_str());BEntry e;while(d.GetNextEntry(&e,false)==B_OK){if(e.IsDirectory()) {BPath path;e.GetPath(&path);clean(path.Path());}e.Remove();}rmdir(p.c_str());}
void *run(void *) {
 try{
  char pattern[]="/tmp/Listi-archive-jobs-XXXXXX";std::string root=mkdtemp(pattern);
  auto settings=LoadArchiveSettings(root+"/settings");
  check(settings.what!='none',"new archive settings are not the missing-file sentinel");
  settings.SetString("default","7z");settings.SetBool("ask",false);settings.SetBool("favorite:7z",true);settings.SetInt32("level:7z",3);
  SaveArchiveSettings(root+"/settings",settings);auto restored=LoadArchiveSettings(root+"/settings");
  check(std::string(restored.GetString("default",""))=="7z" && !restored.GetBool("ask",true) && restored.GetBool("favorite:7z",false) && restored.GetInt32("level:7z",-1)==3,"format preferences survive save and reload");
  restored.SetBool("ask",true);SaveArchiveSettings(root+"/settings",restored);check(LoadArchiveSettings(root+"/settings").GetBool("ask",false),"ask every time remains explicit with a saved default");
  auto originalSettings=read(root+"/settings");restored.SetInt32("version",99);bool invalid=false;
  try{SaveArchiveSettings(root+"/settings",restored);}catch(...){invalid=true;}
  check(invalid && read(root+"/settings")==originalSettings,"invalid settings cannot replace valid saved choices");
  write(root+"/bad-settings","broken");invalid=false;try{LoadArchiveSettings(root+"/bad-settings");}catch(...){invalid=true;}
  check(invalid && read(root+"/bad-settings")=="broken","corrupt settings report an error without overwriting the file");
  mkdir((root+"/in").c_str(),0700);mkdir((root+"/out").c_str(),0700);
  std::string input=root+"/in",out=root+"/out";
  mkdir((input+"/folder").c_str(),0700);mkdir((input+"/folder/empty").c_str(),0700);
  write(input+"/folder/æ name ; $(no).txt","payload\n");write(input+"/empty","");
  auto formats=WritableArchiveFormats();check(!formats.empty(),"optional system writer available");
  std::string archive;
  for(const auto &f:formats){
    auto job=PrepareArchiveJob(false,{input+"/folder",input+"/empty"});job.archiveFormat=f.id;
    SetArchiveDestination(job,out,"roundtrip"+f.extension);
    job=DecodeJob(EncodeJob(job,0));auto result=Run(job,{});
    if(!result.errors.empty())std::cerr<<result.errors.front()<<std::endl;
    check(result.completed==1 && result.errors.empty(),f.id+" creates archive");
    auto extract=PrepareArchiveJob(true,{job.destination});SetArchiveDestination(extract,out,"extract-"+f.id);
    auto unpack=Run(extract,{});if(!unpack.errors.empty())std::cerr<<unpack.errors.front()<<std::endl;
    check(unpack.completed==1 && unpack.errors.empty(),f.id+" extracts transactionally");
    check(read(extract.destination+"/folder/æ name ; $(no).txt")=="payload\n",f.id+" Unicode and literal shell characters round trip");
    check(exists(extract.destination+"/folder/empty") && exists(extract.destination+"/empty"),f.id+" empty file and empty folder preserved");
    auto again=Run(job,{});check(!again.errors.empty() && read(extract.destination+"/folder/æ name ; $(no).txt")=="payload\n",f.id+" existing archive is not replaced");
    for(int level:{0,9}) {
      auto variant=job;variant.archiveLevel=level;SetArchiveDestination(variant,out,f.id+std::to_string(level)+f.extension);
      auto done=Run(variant,{});if(!done.errors.empty())std::cerr<<done.errors.front()<<std::endl;
      check(done.completed==1 && done.errors.empty(),f.id+" compression level "+std::to_string(level));
    }
    archive=job.destination;
  }
  for(const auto &format:formats)if(format.encryption){
    const std::string password="Listi-create-test-Æ";
    auto encrypted=PrepareArchiveJob(false,{input+"/folder",input+"/empty"});
    encrypted.archiveFormat=format.id;encrypted.archiveLevel=7;
    encrypted.archiveEncryptNames=format.encryptedNames;
    encrypted.archivePasswordToken=HoldArchiveCreationPassword(secret(password.c_str()));
    SetArchiveDestination(encrypted,out,"encrypted-"+format.id+format.extension);
    auto message=EncodeJob(encrypted,0);std::vector<char> flattened(message.FlattenedSize());
    check(message.Flatten(flattened.data(),flattened.size())==B_OK &&
      std::search(flattened.begin(),flattened.end(),password.begin(),password.end())==flattened.end(),
      format.id+" job snapshot excludes password bytes");
    encrypted=DecodeJob(message);
    auto made=RunArchiveJob(encrypted,{});
    if(!made.errors.empty())std::cerr<<made.errors.front()<<std::endl;
    check(made.completed==1 && made.errors.empty(),format.id+" AES-256 archive creation");
    int prompts=0;ForgetArchivePasswords();
    SetArchivePasswordPrompt([&](const std::string &,bool,const TransferControl &){++prompts;return secret(password.c_str());});
    auto encryptedURL=OpenArchive(encrypted.destination);
    auto top=ListFiles(encryptedURL);
    auto privateEntry=ChildLocation(ChildLocation(encryptedURL,"folder"),"æ name ; $(no).txt");
    check(top.size()==2 && vread(privateEntry)=="payload\n" && prompts==1,
      format.id+" encrypted archive requires and accepts password");
    auto unpack=PrepareArchiveJob(true,{encrypted.destination});
    SetArchiveDestination(unpack,out,"decrypted-"+format.id);
    auto extracted=RunArchiveJob(unpack,{});
    check(extracted.completed==1 && extracted.errors.empty() &&
      read(unpack.destination+"/folder/æ name ; $(no).txt")=="payload\n",
      format.id+" encrypted round trip exact");
    auto missing=PrepareArchiveJob(false,{input+"/empty"});missing.archiveFormat=format.id;
    missing.archivePasswordToken=encrypted.archivePasswordToken;
    SetArchiveDestination(missing,out,"missing-secret-"+format.id+format.extension);
    auto unavailable=RunArchiveJob(missing,{});
    check(unavailable.cancelled && !exists(missing.destination),format.id+" consumed secret token cannot be reused");
    auto stopped=PrepareArchiveJob(false,{input+"/folder"});stopped.archiveFormat=format.id;
    stopped.archivePasswordToken=HoldArchiveCreationPassword(secret(password.c_str()));
    stopped.archiveEncryptNames=format.encryptedNames;
    SetArchiveDestination(stopped,out,"cancel-encrypted-"+format.id+format.extension);
    bool wrote=false;Hooks stopAfterBytes;
    stopAfterBytes.progress=[&](const std::string &,uint64_t){wrote=true;};
    stopAfterBytes.proceed=[&]{return !wrote;};
    auto cancelledArchive=RunArchiveJob(stopped,stopAfterBytes);
    check(cancelledArchive.cancelled && !exists(stopped.destination),
      format.id+" encrypted cancel publishes no partial archive");
    auto changedEncrypted=PrepareArchiveJob(false,{input+"/folder"});changedEncrypted.archiveFormat=format.id;
    changedEncrypted.archivePasswordToken=HoldArchiveCreationPassword(secret(password.c_str()));
    changedEncrypted.archiveEncryptNames=format.encryptedNames;
    SetArchiveDestination(changedEncrypted,out,"changed-encrypted-"+format.id+format.extension);
    bool edited=false;Hooks editDuringRead;
    editDuringRead.progress=[&](const std::string &,uint64_t){if(!edited){edited=true;write(input+"/folder/æ name ; $(no).txt","changed");}};
    auto changedResult=RunArchiveJob(changedEncrypted,editDuringRead);
    check(!changedResult.errors.empty() && !exists(changedEncrypted.destination),
      format.id+" encrypted source change publishes no archive");
    write(input+"/folder/æ name ; $(no).txt","payload\n");
  }
  ForgetArchivePasswords();
  auto url=OpenArchive(archive);
  auto subset=PrepareArchiveJob(true,{url+"folder",url+"empty"});SetArchiveDestination(subset,out,"selected-members");
  auto subsetResult=Run(subset,{});
  check(subsetResult.completed==1 && read(subset.destination+"/folder/æ name ; $(no).txt")=="payload\n" && exists(subset.destination+"/empty"),"multiple selected archive entries extract together");
  auto late=PrepareArchiveJob(true,{archive});SetArchiveDestination(late,out,"late-directory");Hooks lateHook;bool lateMade=false;
  lateHook.progress=[&](auto &,uint64_t){if(!lateMade){lateMade=true;mkdir(late.destination.c_str(),0700);write(late.destination+"/existing","preserve");}};
  check(!Run(late,lateHook).errors.empty() && read(late.destination+"/existing")=="preserve","late extraction destination is preserved");
  write(input+"/C:unsafe","payload");auto drive=PrepareArchiveJob(false,{input+"/C:unsafe"});SetArchiveDestination(drive,out,"drive.zip");
  check(!Run(drive,{}).errors.empty()&&!exists(drive.destination),"unsafe drive-style archive names fail explicitly");unlink((input+"/C:unsafe").c_str());
  auto create=[&](std::string name){auto j=PrepareArchiveJob(false,{input+"/folder"});SetArchiveDestination(j,out,name);return j;};
  auto j=create("cancel.zip");Hooks cancel;cancel.proceed=[] {return false;};
  check(Run(j,cancel).cancelled&&!exists(j.destination),"cancel creation leaves no output");
  auto ex=PrepareArchiveJob(true,{archive});SetArchiveDestination(ex,out,"cancel-extract");
  check(Run(ex,cancel).cancelled&&!exists(ex.destination),"cancel extraction leaves no output");
  Hooks midCancel;bool progressed=false;midCancel.progress=[&](auto &,uint64_t){progressed=true;};midCancel.proceed=[&]{return !progressed;};
  j=create("mid-cancel.zip");check(Run(j,midCancel).cancelled&&!exists(j.destination),"cancel after bytes written removes staging");
  progressed=false;SetArchiveDestination(ex,out,"mid-extract");check(Run(ex,midCancel).cancelled&&!exists(ex.destination),"cancel after extraction bytes removes staging");
  auto source=PrepareArchiveJob(false,{input+"/empty"});rename((input+"/empty").c_str(),(input+"/original").c_str());write(input+"/empty","replacement");
  bool denied=false;try{SetArchiveDestination(source,out,"replaced.zip");}catch(...){denied=true;}
  check(denied,"replaced selected file rejected before queueing");
  j=create("replaced-dest.zip");rename(out.c_str(),(root+"/old-out").c_str());mkdir(out.c_str(),0700);
  check(!Run(j,{}).errors.empty()&&!exists(j.destination),"replaced destination rejected");
  rmdir(out.c_str());rename((root+"/old-out").c_str(),out.c_str());
  j=create("concurrent.zip");bool changed=false;Hooks conflict;conflict.progress=[&](auto &,uint64_t){if(!changed){changed=true;write(j.destination,"keep me");}};
  check(!Run(j,conflict).errors.empty()&&read(j.destination)=="keep me","late target creation is never overwritten");
  j=create("changed.zip");Hooks mutate;changed=false;mutate.progress=[&](auto &,uint64_t){if(!changed){changed=true;write(input+"/folder/æ name ; $(no).txt","changed during read");}};
  check(!Run(j,mutate).errors.empty()&&!exists(j.destination),"changed source aborts publication");
  j=create("inside.zip");SetArchiveDestination(j,input+"/folder","inside.zip");
  check(!Run(j,{}).errors.empty()&&!exists(j.destination),"output inside selected tree rejected");
  symlink("/boot/home",(input+"/folder/link").c_str());j=create("links.zip");
  check(!Run(j,{}).errors.empty()&&!exists(j.destination),"links are not followed or silently omitted");unlink((input+"/folder/link").c_str());
  write(out+"/broken.zip","PK broken");ex=PrepareArchiveJob(true,{out+"/broken.zip"});SetArchiveDestination(ex,out,"broken-extract");
  check(!Run(ex,{}).errors.empty()&&!exists(ex.destination),"damaged archive does not publish partial folder");
  BDirectory d(out.c_str());BEntry e;bool leftover=false;while(d.GetNextEntry(&e,false)==B_OK){char n[B_FILE_NAME_LENGTH];e.GetName(n);if(std::string(n).find(".Listi-archive-")==0)leftover=true;}
  check(!leftover,"staging removed after success, error and cancel");
  clean(root);std::cout<<"ARCHIVE JOBS COMPLETE "<<passed<<std::endl;return nullptr;
 }catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return reinterpret_cast<void*>(1);}
}
int main(){BApplication app("application/x-vnd.Listi-ArchiveJobsTest");pthread_attr_t attr;pthread_attr_init(&attr);pthread_attr_setstacksize(&attr,256*1024);pthread_t thread;pthread_create(&thread,&attr,run,nullptr);void *result;pthread_join(thread,&result);pthread_attr_destroy(&attr);return result?1:0;}
