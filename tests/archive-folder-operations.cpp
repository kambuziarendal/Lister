// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
std::string content(const std::string&p){std::string s;ReadFile(p,[&](const char*b,size_t n){s.append(b,n);});return s;}
void clean(const std::string&p){BDirectory d(p.c_str());BEntry e;while(d.GetNextEntry(&e,false)==B_OK){struct stat s;e.GetStat(&s);if(S_ISDIR(s.st_mode)){BPath path;e.GetPath(&path);clean(path.Path());}e.Remove();}rmdir(p.c_str());}
int main(){
 BApplication app("application/x-vnd.Listi-archive-folders-test");
 char pattern[]="/tmp/Listi-archive-folders-XXXXXX";auto made=mkdtemp(pattern);if(!made)return 2;std::string root=made;
 mkdir((root+"/in").c_str(),0700);std::ofstream(root+"/in/original.txt")<<"retained";std::ofstream(root+"/in/new.txt")<<"added";
 int checks=0,failures=0;auto check=[&](bool ok,const std::string&label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;};
 for(const auto &format:WritableArchiveFormats()){
  std::cout<<"FORMAT "<<format.id<<std::endl;
  std::vector<std::string> sources{root+"/in/original.txt"};
  if(format.id=="cab"){
   mkdir((root+"/in/new-folder").c_str(),0700);
   std::ofstream(root+"/in/new-folder/keep.txt")<<"seed";
   sources.push_back(root+"/in/new-folder");
  }
  auto create=PrepareArchiveJob(false,sources);create.archiveFormat=format.id;SetArchiveDestination(create,root,"archive"+format.extension);
  auto result=Run(create,{});check(result.completed==1&&result.errors.empty(),format.id+" create");if(!result.errors.empty()){std::cerr<<result.errors[0]<<std::endl;continue;}
  try {
   const auto url=OpenArchive(create.destination);auto target=PrepareArchiveEditTarget(create.destination);
   check(target.archiveFormat==format.id,format.id+" detected from container, not file suffix");
   auto folder=ChildLocation(url,"new-folder");
   auto mkdirJob=PrepareArchiveMutation(Job{Action::MakeDirectory,{},format.id=="cab"?ChildLocation(url,"empty"):folder});
   result=Run(DecodeJob(EncodeJob(mkdirJob,0)),{});FileEntry entry;
   if(format.id=="cab")
    check(!result.errors.empty()&&!StatFile(ChildLocation(url,"empty"),entry),"cab refuses unrepresentable empty directory");
   else
    check(result.completed==1&&StatFile(folder,entry)&&entry.directory,format.id+" create internal folder");
   auto add=PrepareArchiveCopy(Job{Action::Copy,{root+"/in/new.txt"},folder});result=Run(add,{});
   auto file=ChildLocation(folder,"new.txt");check(result.completed==1&&content(file)=="added",format.id+" copy into internal folder");
   auto renamed=ChildLocation(folder,"renamed.txt");auto renameJob=PrepareArchiveMutation(Job{Action::Rename,{file},renamed});result=Run(DecodeJob(EncodeJob(renameJob,0)),{});
   check(result.completed==1&&!StatFile(file,entry)&&content(renamed)=="added",format.id+" rename internal file");
   auto move=PrepareArchiveMutation(Job{Action::Move,{renamed},url});result=Run(move,{});
   auto moved=ChildLocation(url,"renamed.txt");check(result.completed==1&&!StatFile(renamed,entry)&&content(moved)=="added",format.id+" move within same archive");
   auto copy=PrepareArchiveCopy(Job{Action::Copy,{moved},folder});result=Run(copy,{});
   check(result.completed==1&&content(moved)=="added"&&content(renamed)=="added",format.id+" copy within same archive");
   auto remove=PrepareArchiveMutation(Job{Action::Delete,{moved},""});result=Run(remove,{});
   check(result.completed==1&&!StatFile(moved,entry)&&content(renamed)=="added"&&content(ChildLocation(url,"original.txt"))=="retained",format.id+" delete selected entry only");
   check(PrepareArchiveEditTarget(create.destination).archiveFormat==format.id,format.id+" format and compression wrapper preserved after all edits");
   auto collision=PrepareArchiveMutation(Job{Action::Rename,{renamed},ChildLocation(url,"original.txt")});
   result=Run(collision,{});check(!result.errors.empty()&&!result.completed&&content(renamed)=="added"&&content(ChildLocation(url,"original.txt"))=="retained",format.id+" rename conflict preserves both entries");
  }catch(const std::exception&e){check(false,format.id+" exception: "+e.what());}
 }
 clean(root);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
