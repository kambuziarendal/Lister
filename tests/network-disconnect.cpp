// SPDX-License-Identifier: MIT
// Requires the task-owned SFTP fixture that closes the transport mid-read.
#include "FileOperation.h"
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Listi;
std::string read(const std::string&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 char pattern[]="/tmp/Listi-disconnect-XXXXXX";char *made=mkdtemp(pattern);if(!made)return 2;std::string root=made;
 const std::string server="sftp://127.0.0.1:2222/";
 Connection auth;auth.username="listitest";auth.password="fixture-only";auth.knownHosts=argv[1];SetConnection(server,auth);
 std::ofstream(root+"/disconnect.bin")<<"original retained";
 Hooks h;h.conflict=[](const std::string&){return Decision::Replace;};int completed=0;h.fileDone=[&](const std::string&,const std::string&){++completed;};
 Job job{Action::Copy,{ChildLocation(server,"disconnect.bin")},root};FreezeLocalJob(job,root);
 auto result=Run(job,h);bool safe=!result.errors.empty()&&!result.completed&&!completed&&read(root+"/disconnect.bin")=="original retained";
 std::cout<<(safe?"PASS ":"FAIL ")<<"real SFTP transport loss preserves original destination and emits no completion"<<std::endl;
 bool recovered=false;try{std::string s;ReadFile(ChildLocation(server,"hello world.txt"),[&](const char*p,size_t n){s.append(p,n);});recovered=s=="Listi network fixture\n";}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;}
 std::cout<<(recovered?"PASS ":"FAIL ")<<"a fresh SFTP operation succeeds after connection loss"<<std::endl;
 unlink((root+"/disconnect.bin").c_str());rmdir(root.c_str());return safe&&recovered?0:1;
}
