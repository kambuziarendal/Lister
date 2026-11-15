// SPDX-License-Identifier: MIT
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
int main() {
  char tmp[]="/tmp/Listi-snapshot-XXXXXX";char *root=mkdtemp(tmp);if(!root)return 2;
  std::string base=root,src=base+"/file",dst=base+"/dest";mkdir(dst.c_str(),0755);
  int failures=0,checks=0;
  auto check=[&](bool ok,const char*text){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<text<<std::endl;};
  for(auto action:{Lister::Action::Trash,Lister::Action::Delete,Lister::Action::Rename,Lister::Action::Copy,Lister::Action::Move}) {
    {std::ofstream f(src);f<<"original";}
    Lister::Job job{action,{src},dst};
    Lister::FreezeLocalJob(job,action==Lister::Action::Rename?base:action==Lister::Action::Copy||action==Lister::Action::Move?dst:"");
    auto captured=Lister::DecodeJob(Lister::EncodeJob(job,'test'));
    if(action==Lister::Action::Rename)captured.destination=base+"/renamed";
    rename(src.c_str(),(src+".original").c_str());{std::ofstream f(src);f<<"replacement";}
    auto result=Lister::Run(captured,{});
    struct stat st;
    check(!result.errors.empty() && !result.completed && !lstat(src.c_str(),&st),"delayed action rejects replaced source after message roundtrip");
    unlink(src.c_str());unlink((src+".original").c_str());
  }
  for(auto action:{Lister::Action::MakeDirectory,Lister::Action::Rename}) {
    {std::ofstream f(src);f<<"payload";}
    Lister::Job job{action,action==Lister::Action::Rename?std::vector<std::string>{src}:std::vector<std::string>{},dst};
    Lister::FreezeLocalJob(job,dst);auto captured=Lister::DecodeJob(Lister::EncodeJob(job,'test'));
    captured.destination=dst+"/new";
    rename(dst.c_str(),(dst+".original").c_str());mkdir(dst.c_str(),0755);
    auto result=Lister::Run(captured,{});struct stat st;
    check(!result.errors.empty() && !result.completed && lstat(captured.destination.c_str(),&st)!=0,"named operation rejects replaced parent folder");
    rmdir(dst.c_str());rename((dst+".original").c_str(),dst.c_str());unlink(src.c_str());
  }
  Lister::Job valid{Lister::Action::MakeDirectory,{},dst};Lister::FreezeLocalJob(valid,dst);valid.destination=dst+"/new";
  check(Lister::Run(valid,{}).completed==1,"unchanged captured parent permits new folder");rmdir(valid.destination.c_str());
  std::vector<std::string> deep{base+"/deep"};mkdir(deep.back().c_str(),0755);
  for(int i=0;i<66;++i){deep.push_back(deep.back()+"/d");mkdir(deep.back().c_str(),0755);}
  {std::ofstream f(deep.back()+"/payload");f<<"preserve";}
  Lister::Job removal{Lister::Action::Delete,{deep.front()},""};
  auto refused=Lister::Run(removal,{});struct stat deepStat;
  check(!refused.errors.empty() && !refused.completed && !stat((deep.back()+"/payload").c_str(),&deepStat),"deep deletion refused before removing any entry");
  Lister::Job copying{Lister::Action::Copy,{deep.front()},dst};
  auto bounded=Lister::Run(copying,{});
  check(!bounded.errors.empty() && !bounded.completed && !stat((deep.back()+"/payload").c_str(),&deepStat),"deep copy reports bounded failure and preserves source");
  unlink((deep.back()+"/payload").c_str());
  for(auto i=deep.rbegin();i!=deep.rend();++i){rmdir((dst+i->substr(base.size())).c_str());rmdir(i->c_str());}
  rmdir(dst.c_str());rmdir(base.c_str());
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
