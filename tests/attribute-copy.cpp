// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include <Node.h>
#include <TypeConstants.h>
#include <fs_attr.h>
#include <cstdlib>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
int main() {
  char pattern[]="/boot/home/Listi-attributes-XXXXXX";
  char *root=mkdtemp(pattern);if(!root)return 2;
  int checks=0,failures=0;
  auto check=[&](bool ok,const char *label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;};
  std::string base=root,source=base+"/source",dest=base+"/dest";
  mkdir(source.c_str(),0755);mkdir(dest.c_str(),0755);
  {std::ofstream f(source+"/file");f<<"payload";}
  BNode folder(source.c_str()),file((source+"/file").c_str());
  attr_info info{};
  int32 number=42;
  const char label[]="folder metadata";
  check(file.WriteAttr("Listi:number",B_INT32_TYPE,0,&number,sizeof(number))==sizeof(number),"native fixture has typed integer file attribute");
  check(folder.WriteAttr("Listi:label",B_STRING_TYPE,0,label,sizeof(label))==sizeof(label),"native fixture has string folder attribute");
  std::string large(150000,'x');file.WriteAttr("Listi:large",B_RAW_TYPE,0,large.data(),large.size());
  Lister::Job job;job.sources={source};job.destination=dest;
  auto result=Lister::Run(job,{});
  check(result.errors.empty(),"copy completes without metadata error");
  BNode copiedFile((dest+"/source/file").c_str()),copiedFolder((dest+"/source").c_str());
  int32 copiedNumber=0;char copiedLabel[sizeof(label)]={};
  check(copiedFile.GetAttrInfo("Listi:number",&info)==B_OK && info.size==sizeof(number) && info.type==B_INT32_TYPE && copiedFile.ReadAttr("Listi:number",B_INT32_TYPE,0,&copiedNumber,sizeof(copiedNumber))==sizeof(copiedNumber) && copiedNumber==number,"file attribute type and integer value preserved");
  check(copiedFolder.GetAttrInfo("Listi:label",&info)==B_OK && info.type==B_STRING_TYPE && copiedFolder.ReadAttr("Listi:label",B_STRING_TYPE,0,copiedLabel,sizeof(copiedLabel))==sizeof(copiedLabel) && std::string(copiedLabel)==label,"folder attribute type and value preserved");
  std::string actual(large.size(),' ');
  check(copiedFile.ReadAttr("Listi:large",B_RAW_TYPE,0,actual.data(),actual.size())==ssize_t(actual.size()) && actual==large,"multi-chunk attribute preserved");
  Lister::Job cleanup;cleanup.action=Lister::Action::Delete;cleanup.sources={base};
  auto removed=Lister::Run(cleanup,{});
  check(removed.errors.empty(),"owned fixture cleaned");
  std::cout<<checks<<" attribute checks, "<<failures<<" failures\n";
  return failures?1:0;
}
