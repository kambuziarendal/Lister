#include "ArchiveJobs.h"
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
static uint64 calls=0;static bool counting=false;
extern "C" int __real_lstat(const char*,struct stat*);
extern "C" int __wrap_lstat(const char* p,struct stat* st){if(counting)++calls;return __real_lstat(p,st);}
void clean(const std::string &p){BDirectory d(p.c_str());BEntry e;while(d.GetNextEntry(&e,false)==B_OK){struct stat s;e.GetStat(&s);if(S_ISDIR(s.st_mode)){BPath path;e.GetPath(&path);clean(path.Path());}e.Remove();}rmdir(p.c_str());}
int main(){BApplication app("application/x-vnd.Listi-AuditScaleProbe");char base[]="/boot/home/Listi-scale-probe-XXXXXX";if(!mkdtemp(base))return 2;
 try{for(int count:{64,128}){
 std::string root=std::string(base)+"/case"+std::to_string(count),tree=root+"/tree",out=root+"/out";
 mkdir(root.c_str(),0700);mkdir(tree.c_str(),0700);mkdir(out.c_str(),0700);
 for(int i=0;i<count;++i)std::ofstream(tree+"/item"+std::to_string(i))<<std::string(1024,'x');
 auto create=PrepareArchiveJob(false,{tree});create.archiveFormat="zip";SetArchiveDestination(create,root,"fixture");auto made=Run(create,{});if(made.completed!=1||!made.errors.empty())return 3;
 auto archive=OpenArchive(create.destination);auto job=PrepareArchiveMove(Job{Action::Move,{ChildLocation(archive,"tree")},out});
 calls=0;counting=true;bigtime_t start=system_time();auto result=Run(job,{});auto elapsed=system_time()-start;counting=false;
 if(result.completed!=1||!result.errors.empty()){for(auto&e:result.errors)std::cerr<<e<<std::endl;return 4;}
 int verified=0;for(int i=0;i<count;++i){std::string content;ReadFile(out+"/tree/item"+std::to_string(i),[&](const char*p,size_t n){content.append(p,n);});if(content==std::string(1024,'x'))++verified;}
 FileEntry removed;bool sourceGone=!StatFile(ChildLocation(archive,"tree"),removed);
 std::cout<<"files="<<count<<" bytes_each=1024 lstat_calls="<<calls<<" elapsed_us="<<elapsed<<" verified="<<verified<<" source_removed="<<sourceGone<<std::endl;
 if(verified!=count||!sourceGone)return 5;
 }clean(base);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 6;}}
