// SPDX-License-Identifier: MIT
// Run only against the task-owned SMB/FTP/SFTP failure fixture. The host driver
// verifies server bytes and recovery names independently of this client.
#include "FileOperation.h"
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <OS.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Lister;
std::string bytes(const std::string&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv) {
 if(argc!=4)return 2;
 std::string protocol=argv[1],mode=argv[2];
 std::string server=protocol=="smb"?"smb://127.0.0.1:1445/files":protocol=="ftp"?"ftp://127.0.0.1:2121/":"sftp://127.0.0.1:2222/";
 Connection auth;auth.username="listitest";auth.password="fixture-only";auth.knownHosts=argv[3];SetConnection(server,auth);
 char temp[]="/tmp/Listi-net-break-XXXXXX";if(!mkdtemp(temp))return 2;std::string root=temp;
 std::string payload(2097152,'n');std::ofstream(root+"/input",std::ios::binary)<<payload;
 std::ofstream(root+"/download")<<"original retained";
 Hooks h;h.conflict=[](const std::string&){return Decision::Replace;};int done=0;
 h.fileDone=[&](const std::string&,const std::string&){++done;};
 bool cancelling=mode=="cancel-download";bigtime_t cancelledAt=0;uint64_t received=0;bool proceed=true;
 h.proceed=[&]{return proceed;};
 h.progress=[&](const std::string&,uint64_t n){received+=n;if(cancelling&&received>=65536&&proceed){cancelledAt=system_time();proceed=false;}};
 Job job=(mode=="download"||cancelling)?Job{Action::Copy,{ChildLocation(server,"download")},root}:Job{Action::Copy,{root+"/input"},server};
 FreezeLocalJob(job,job.destination);auto start=system_time();auto r=Run(job,h);
 std::cout<<"RESULT protocol="<<protocol<<" mode="<<mode<<" completed="<<r.completed<<" notifications="<<done<<" cancelled="<<r.cancelled<<" elapsed_us="<<system_time()-start<<std::endl;
 for(auto&e:r.errors)std::cout<<"ERROR "<<e<<std::endl;
 int checks=0,failed=0;auto check=[&](bool ok,const char*label){++checks;failed+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;};
 check(mode=="none"?(r.completed==1&&done==1&&r.errors.empty()):(!r.completed&&!done&&(!r.errors.empty()||r.cancelled)),"operation reports an explicit terminal outcome");
 check(bytes(root+"/input")==payload,"local upload source retains exact bytes");
 if(mode=="download"||cancelling)check(bytes(root+"/download")=="original retained","broken download retains existing local destination");
 if(cancelling){auto latency=system_time()-cancelledAt;std::cout<<"CANCEL latency_us="<<latency<<std::endl;check(cancelledAt&&latency<5000000,"active download cancels within measured five-second test budget");}
 bool recovered=false;try{std::string s;ReadFile(ChildLocation(server,"hello"),[&](const char*p,size_t n){s.append(p,n);});recovered=s=="hello";}catch(const std::exception&e){std::cout<<"RECOVERY "<<e.what()<<std::endl;}
 check(recovered,"fresh operation reconnects after interrupted request");
 unlink((root+"/input").c_str());unlink((root+"/download").c_str());rmdir(root.c_str());
 std::cout<<checks<<" checks, "<<failed<<" failures"<<std::endl;return failed?1:0;
}
