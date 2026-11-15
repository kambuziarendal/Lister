// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "VirtualFile.h"
#include <Application.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Lister;
std::string bytes(const std::string &p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing expected file: "+p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
 BApplication app("application/x-vnd.Listi-ArchiveJobFixtures");if(argc!=3)return 2;
 char tmp[]="/tmp/Listi-archive-fixtures-XXXXXX";std::string out=mkdtemp(tmp),root=argv[1];
 std::ifstream input(argv[2]);if(!input)return 2;int count=0,failed=0;std::string line;
 while(std::getline(input,line)){
  std::istringstream fields(line);std::string file,entry,expected;
  std::getline(fields,file,'\t');std::getline(fields,entry,'\t');std::getline(fields,expected);
  ++count;
  try{
   auto source=OpenArchive(root+"/"+file)+EncodePath(entry);
   auto job=PrepareArchiveJob(true,{source});SetArchiveDestination(job,out,std::to_string(count));
   auto result=Run(job,{});if(!result.errors.empty())throw std::runtime_error(result.errors.front());
   if(result.completed!=1 || bytes(job.destination+"/"+LocationLeaf(entry))!=bytes(root+"/"+expected))throw std::runtime_error("Extracted content differs");
   unlink((job.destination+"/"+LocationLeaf(entry)).c_str());rmdir(job.destination.c_str());
   std::cout<<"PASS queued selected-member extraction "<<file<<std::endl;
  }catch(const std::exception &e){++failed;std::cerr<<"FAIL "<<file<<": "<<e.what()<<std::endl;}
 }
 for(const char *file:{"unsafe.zip","links.tar","broken.7z","test_read_format_rar5_encrypted.rar","test_read_format_rar5_multiarchive.part01.rar"}){
   ++count;auto job=PrepareArchiveJob(true,{root+"/"+file});SetArchiveDestination(job,out,"invalid-"+std::to_string(count));
   auto result=Run(job,{});struct stat s;bool ok=!result.errors.empty()&&lstat(job.destination.c_str(),&s)!=0;
   failed+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<"negative archive job "<<file<<std::endl;
 }
 rmdir(out.c_str());std::cout<<count<<" archive fixture jobs, "<<failed<<" failures\n";return failed?1:0;
}
