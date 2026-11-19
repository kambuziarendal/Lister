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
  Listi::Job job;job.sources={source};job.destination=dest;
  auto result=Listi::Run(job,{});
  check(result.errors.empty(),"copy completes without metadata error");
  BNode copiedFile((dest+"/source/file").c_str()),copiedFolder((dest+"/source").c_str());
  int32 copiedNumber=0;char copiedLabel[sizeof(label)]={};
  check(copiedFile.GetAttrInfo("Listi:number",&info)==B_OK && info.size==sizeof(number) && info.type==B_INT32_TYPE && copiedFile.ReadAttr("Listi:number",B_INT32_TYPE,0,&copiedNumber,sizeof(copiedNumber))==sizeof(copiedNumber) && copiedNumber==number,"file attribute type and integer value preserved");
  check(copiedFolder.GetAttrInfo("Listi:label",&info)==B_OK && info.type==B_STRING_TYPE && copiedFolder.ReadAttr("Listi:label",B_STRING_TYPE,0,copiedLabel,sizeof(copiedLabel))==sizeof(copiedLabel) && std::string(copiedLabel)==label,"folder attribute type and value preserved");
  std::string actual(large.size(),' ');
  check(copiedFile.ReadAttr("Listi:large",B_RAW_TYPE,0,actual.data(),actual.size())==ssize_t(actual.size()) && actual==large,"multi-chunk attribute preserved");
  const char conflicting[]="other metadata!";
  check(copiedFolder.WriteAttr("Listi:label",B_STRING_TYPE,0,conflicting,sizeof(conflicting))==sizeof(conflicting),
        "merged destination has distinct folder metadata");
  Listi::Job move{Listi::Action::Move,{source},dest};
  Listi::Hooks replace;
  replace.conflict=[](const std::string&){return Listi::Decision::Replace;};
  auto merged=Listi::Run(move,replace);
  check(!merged.errors.empty() && !merged.completed &&
            access((source+"/file").c_str(),F_OK)==0 &&
            copiedFolder.ReadAttr("Listi:label",B_STRING_TYPE,0,copiedLabel,sizeof(copiedLabel))==sizeof(conflicting) &&
            std::string(copiedLabel)==conflicting,
        "moving onto a folder with different attribute bytes preserves both trees");
  check(copiedFolder.WriteAttr("Listi:label",B_STRING_TYPE,0,label,sizeof(label))==sizeof(label),
        "restore matching destination attribute");
  merged=Listi::Run(move,replace);
  check(merged.errors.empty() && merged.completed==1 && access(source.c_str(),F_OK)!=0,
        "matching folder attributes allow a merged move");
  // BFS can retain a zero-length typed attribute after truncating an
  // inode-backed attribute. A zero-byte WriteAttr alone may do nothing.
  const std::string tagged = base + "/empty-tagged";
  const std::string original = dest + "/empty-tagged";
  { std::ofstream(tagged) << "replacement"; }
  { std::ofstream(original) << "original"; }
  BNode taggedNode(tagged.c_str());
  constexpr const char *emptyName = "Listi:empty";
  check(taggedNode.WriteAttr(emptyName, B_RAW_TYPE, 0, large.data(),
                            large.size()) == ssize_t(large.size()),
        "create file-backed typed attribute");
  int truncated = fs_open_attr(tagged.c_str(), emptyName, B_RAW_TYPE,
                               O_WRONLY | O_TRUNC | O_CREAT);
  check(truncated >= 0 && fs_close_attr(truncated) == 0 &&
            taggedNode.GetAttrInfo(emptyName, &info) == B_OK &&
            info.type == B_RAW_TYPE && info.size == 0,
        "truncation leaves a present zero-byte typed BFS attribute");

  Listi::Job overwrite{Listi::Action::Copy, {tagged}, dest};
  auto refused = Listi::Run(overwrite, replace);
  std::string stored;
  std::ifstream(original) >> stored;
  check(refused.completed == 0 && !refused.errors.empty() &&
            refused.errors.front().find(emptyName) != std::string::npos &&
            stored == "original" && taggedNode.GetAttrInfo(emptyName, &info) == B_OK &&
            info.size == 0 && info.type == B_RAW_TYPE,
        "refused empty attribute leaves original destination and source metadata intact");
  // A same-volume folder merge stages its children before removing them.
  // Haiku links can have their own attributes even when their target is absent.
  const std::string linkFolder = base + "/links";
  const std::string mergedFolder = dest + "/links";
  const std::string originalLink = linkFolder + "/broken";
  const std::string movedLink = mergedFolder + "/broken";
  mkdir(linkFolder.c_str(), 0755);
  mkdir(mergedFolder.c_str(), 0755);
  check(symlink("not-present", originalLink.c_str()) == 0,
        "create a dangling symbolic link");
  BNode linkNode(originalLink.c_str());
  check(linkNode.WriteAttr("Listi:number", B_INT32_TYPE, 0, &number,
                           sizeof(number)) == sizeof(number),
        "attach a typed attribute to the symbolic link itself");
  struct timespec linkTimes[2] = {{1100000000, 0}, {1200000000, 0}};
  check(utimensat(AT_FDCWD, originalLink.c_str(), linkTimes, AT_SYMLINK_NOFOLLOW) == 0,
        "set the time on the symbolic link itself");
  auto linkMerge = Listi::Run({Listi::Action::Move, {linkFolder}, dest}, {});
  BNode movedNode(movedLink.c_str());
  int32 movedNumber = 0;
  char linkText[64];
  ssize_t length = readlink(movedLink.c_str(), linkText, sizeof(linkText));
  struct stat movedStat;
  check(linkMerge.completed == 1 && linkMerge.errors.empty() &&
            lstat(movedLink.c_str(), &movedStat) == 0 &&
            movedStat.st_mtim.tv_sec == linkTimes[1].tv_sec &&
            length > 0 && std::string(linkText, length) == "not-present" &&
            movedNode.ReadAttr("Listi:number", B_INT32_TYPE, 0, &movedNumber,
                               sizeof(movedNumber)) == sizeof(movedNumber) &&
            movedNumber == number && access(linkFolder.c_str(), F_OK) != 0,
        "merged move preserves a dangling link and its typed metadata");
  Listi::Job cleanup;cleanup.action=Listi::Action::Delete;cleanup.sources={base};
  auto removed=Listi::Run(cleanup,{});
  check(removed.errors.empty(),"owned fixture cleaned");
  std::cout<<checks<<" attribute checks, "<<failures<<" failures\n";
  return failures?1:0;
}
