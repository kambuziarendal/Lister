// SPDX-License-Identifier: MIT
// Native adversarial transfer regressions using actual archive and local I/O.
#include "ArchiveJobs.h"
#include "JobSnapshot.h"
#include "FileDrop.h"
#include "FileClipboard.h"
#include <Application.h>
#include "VirtualFile.h"
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Lister;
namespace {
void writeFile(const std::string &p,const std::string &s){std::ofstream(p)<<s;}
std::string readFile(const std::string &p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
bool exists(const std::string &p){struct stat s;return !lstat(p.c_str(),&s);}
void clean(const std::string &p){BDirectory d(p.c_str());BEntry e;while(d.GetNextEntry(&e,false)==B_OK){struct stat s;e.GetStat(&s);if(S_ISDIR(s.st_mode)){BPath path;e.GetPath(&path);clean(path.Path());}e.Remove();}rmdir(p.c_str());}
}
int main(){
 BApplication app("application/x-vnd.Listi-transfer-races-test");
 char temp[]="/tmp/Listi-transfer-races-XXXXXX";auto made=mkdtemp(temp);if(!made)return 2;
 std::string root=made,input=root+"/input",out=root+"/out";mkdir(input.c_str(),0700);mkdir(out.c_str(),0700);
 writeFile(input+"/payload",std::string(300000,'p'));
 mkdir((input+"/folder").c_str(),0700);
 auto pack=PrepareArchiveJob(false,{input+"/payload",input+"/folder"});SetArchiveDestination(pack,root,"source.zip");
 auto packed=Run(pack,{});if(packed.completed!=1)return 2;
 auto member=ChildLocation(OpenArchive(pack.destination),"payload");
 int checks=0,failures=0;auto check=[&](bool ok,const char*name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;};
 // A queued archive download must retain the folder selected by the user.
 Job job{Action::Copy,{member},out};FreezeLocalJob(job,out);
 job=DecodeJob(EncodeJob(job,0));rename(out.c_str(),(out+"-old").c_str());mkdir(out.c_str(),0700);
 auto result=Run(job,{});
 check(!result.errors.empty()&&!result.completed&&!exists(out+"/payload"),"queued download refuses replaced destination folder");
 clean(out);rename((out+"-old").c_str(),out.c_str());
 // Swap the target while bytes are being read, before the local publish call.
 for(bool link:{false,true}){
  bool changed=false;Hooks h;h.progress=[&](const std::string&,uint64_t n){if(n&&!changed){changed=true;rename(out.c_str(),(out+"-old").c_str());if(link){mkdir((out+"-other").c_str(),0700);symlink((out+"-other").c_str(),out.c_str());}else mkdir(out.c_str(),0700);}};
  result=Run({Action::Copy,{member},out},h);
  check(changed&&!result.errors.empty()&&!result.completed&&!exists(out+"/payload"),link?"download rejects parent redirected by symlink during staging":"download rejects parent replaced during staging");
  if(link){unlink(out.c_str());clean(out+"-other");}else clean(out);
  rename((out+"-old").c_str(),out.c_str());
 }
 writeFile(out+"/payload","original");
 bool changed=false;Hooks race;race.progress=[&](const std::string&,uint64_t n){if(n&&!changed){changed=true;writeFile(out+"/payload","concurrent user edit");}};race.conflict=[](const std::string&){return Decision::Replace;};
 result=Run({Action::Copy,{member},out},race);
 check(changed&&!result.errors.empty()&&!result.completed&&readFile(out+"/payload")=="concurrent user edit","download preserves destination edited during staging");
 writeFile(out+"/payload","keep");
 Hooks skip;skip.conflict=[](const std::string&){return Decision::Skip;};result=Run({Action::Copy,{member},out},skip);
 check(result.errors.empty()&&!result.completed&&result.skipped==1&&readFile(out+"/payload")=="keep","skipped download is not counted complete");
 Hooks abort;abort.conflict=[](const std::string&){return Decision::Abort;};result=Run({Action::Copy,{member},out},abort);
 check(result.cancelled&&!result.completed&&readFile(out+"/payload")=="keep","aborted download preserves original bytes");
 Hooks replace;replace.conflict=[](const std::string&){return Decision::Replace;};result=Run({Action::Copy,{member},out},replace);
 check(result.errors.empty()&&result.completed==1&&readFile(out+"/payload")==std::string(300000,'p'),"unchanged target accepts download replacement");
 // Source identity must survive queueing a local-to-network operation too.
 Job upload{Action::Copy,{input+"/payload"},"sftp://fixture.invalid/out"};FreezeLocalJob(upload,upload.destination);
 check(upload.expectedSources.count(input+"/payload")==1,"upload captures local source identity before queueing");
 rename((input+"/payload").c_str(),(input+"/saved-payload").c_str());writeFile(input+"/payload","replaced");
 result=Run(upload,{});
 check(!result.completed&&!result.errors.empty()&&result.errors[0].find("Selected source changed")!=std::string::npos&&readFile(input+"/payload")=="replaced",
   "queued upload rejects replaced source before network I/O");
 // All ordinary user entry points must reach the same guarded archive editor.
 const auto archiveRoot=OpenArchive(pack.destination);
 writeFile(input+"/new.txt","new archive entry");
 Job transfer{Action::Copy,{input+"/new.txt"},archiveRoot};FreezeLocalJob(transfer,archiveRoot);
 auto edit=PrepareArchiveCopy(transfer);result=Run(edit,{});
 check(result.completed==1&&result.errors.empty()&&exists(input+"/new.txt"),"panel copy adds to archive and retains source");
 auto nested=ChildLocation(archiveRoot,"folder");
 auto drop=PrepareFileDrop(MakeFileDrag({input+"/new.txt"}),nested);result=Run(drop,{});
 check(drop.action==Action::EditArchive&&result.completed==1&&result.errors.empty(),"drop adds to selected internal archive folder");
 std::string content;ReadFile(ChildLocation(nested,"new.txt"),[&](const char*p,size_t n){content.append(p,n);});
 check(content=="new archive entry","archive addition contains exact source bytes");
 BClipboard clipboard("Listi-archive-paste-test");
 writeFile(input+"/pasted.txt","clipboard entry");PutFileClipboard({input+"/pasted.txt"},false,&clipboard);
 auto paste=PrepareFilePaste(nested,&clipboard);result=Run(paste.jobs.at(0),{});
 check(paste.lease.empty()&&paste.jobs.size()==1&&paste.jobs[0].action==Action::EditArchive&&result.completed==1,"clipboard Copy/Paste adds to open archive without a move lease");
 PutFileClipboard({input+"/pasted.txt"},true,&clipboard);bool refused=false;
 paste=PrepareFilePaste(nested,&clipboard);
 check(!paste.lease.empty()&&paste.jobs.at(0).action==Action::MoveArchive&&exists(input+"/pasted.txt"),"cut/paste prepares verified move without removing source before acceptance");
 FinishFilePaste(paste,{},&clipboard);
 transfer.action=Action::Move;refused=false;try{PrepareArchiveCopy(transfer);}catch(const std::exception&){refused=true;}
 check(refused&&exists(input+"/new.txt"),"copy helper refuses a move contract without removing source");
 edit=PrepareArchiveCopy(Job{Action::Copy,{input+"/new.txt"},nested});
 const auto originalArchive=readFile(pack.destination);Hooks cancelEdit;cancelEdit.proceed=[] {return false;};result=Run(edit,cancelEdit);
 check(result.cancelled&&readFile(pack.destination)==originalArchive,"cancelled archive addition retains exact original container");
 PutFileClipboard({ChildLocation(nested,"new.txt")},false,&clipboard);
 auto outgoing=PrepareFilePaste(out,&clipboard);result=Run(outgoing.jobs.at(0),{});
 check(result.completed==1&&readFile(out+"/new.txt")=="new archive entry","clipboard Copy/Paste copies archive entry to local folder");
 PutFileClipboard({ChildLocation(nested,"new.txt")},false,&clipboard);
 auto stale=PrepareFilePaste(out,&clipboard);
 auto change=PrepareArchiveMutation(Job{Action::MakeDirectory,{},ChildLocation(archiveRoot,"later")});
 check(Run(change,{}).completed==1,"archive clipboard staleness fixture changes container");
 result=Run(stale.jobs.at(0),{});
 check(!result.errors.empty()&&!result.completed,"queued archive clipboard refuses changed source container");
 refused=false;try{PrepareFilePaste(out,&clipboard);}catch(const std::exception&){refused=true;}
 check(refused,"archive clipboard refuses stale selection before queueing");
 clean(root);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
