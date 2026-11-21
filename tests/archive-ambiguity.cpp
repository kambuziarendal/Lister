// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "DynamicLibrary.h"
#include "VirtualFile.h"
#include <archive.h>
#include <archive_entry.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
int main(){
 char pattern[]="/tmp/Listi-ambiguous-XXXXXX";char *made=mkdtemp(pattern);if(!made)return 2;std::string root=made;
 int checks=0,failures=0;auto check=[&](bool ok,const std::string&name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;};
 DynamicLibrary lib{"libarchive.so","libarchive.so.13"};if(!lib.Available())return 2;
 const std::vector<std::vector<std::string>> cases={{"item","item"},{"folder/item","folder/./item"},{"item","item/child"},{"item/child","item"}};
 for(const auto &format:{std::string("tar"),std::string("zip")})for(size_t i=0;i<cases.size();++i){
  auto path=root+"/case-"+std::to_string(i)+"."+format;
  archive *writer=LIBCALL(lib,archive_write_new)();
  if(format=="zip")LIBCALL(lib,archive_write_set_format_zip)(writer);else LIBCALL(lib,archive_write_set_format_pax_restricted)(writer);
  if(LIBCALL(lib,archive_write_open_filename)(writer,path.c_str())!=ARCHIVE_OK)return 2;
  for(const auto &name:cases[i]){
   auto e=LIBCALL(lib,archive_entry_new)();LIBCALL(lib,archive_entry_set_pathname)(e,name.c_str());LIBCALL(lib,archive_entry_set_filetype)(e,AE_IFREG);LIBCALL(lib,archive_entry_set_perm)(e,0600);LIBCALL(lib,archive_entry_set_size)(e,5);
   if(LIBCALL(lib,archive_write_header)(writer,e)!=ARCHIVE_OK)return 2;
   if(LIBCALL(lib,archive_write_data)(writer,"bytes",5)!=5)return 2;
   LIBCALL(lib,archive_entry_free)(e);
  }
  if(LIBCALL(lib,archive_write_close)(writer)!=ARCHIVE_OK)return 2;LIBCALL(lib,archive_write_free)(writer);
  bool rejected=false;try{ListFiles(OpenArchive(path));}catch(const std::exception&){rejected=true;}
  check(rejected,format+" rejects ambiguous listing "+std::to_string(i));
  auto job=PrepareArchiveJob(true,{path});SetArchiveDestination(job,root,"extract-"+format+std::to_string(i));auto result=Run(job,{});struct stat st;
  check(!result.errors.empty()&&!result.completed&&lstat(job.destination.c_str(),&st)!=0,format+" never publishes ambiguous extraction "+std::to_string(i));
 }
 // Original DOS ZIP member names are CP437, not arbitrary local filename bytes.
 const auto oldZip = root + "/legacy.zip";
 {
   std::ifstream in("tests/fixtures/zip/cp437.zip", std::ios::binary);
   std::ofstream out(oldZip, std::ios::binary);
   out << in.rdbuf();
   if (!out) return 2;
 }
 auto legacyEntries = ListFiles(OpenArchive(oldZip));
 const std::string expected = "Ålesund Æö.txt";
 check(legacyEntries.size() == 1 && legacyEntries[0].name == expected,
       "DOS ZIP exposes a UTF-8 filename");
 auto extract = PrepareArchiveJob(true, {oldZip});
 SetArchiveDestination(extract, root, "legacy");
 auto extracted = Run(extract, {});
 std::string payload;
 std::getline(std::ifstream(extract.destination + "/" + expected), payload);
 check(extracted.completed == 1 && extracted.errors.empty() && payload == "payload-123",
       "DOS ZIP extracts under the decoded name with exact contents");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";
 // Retain small malformed fixtures for diagnosis if a regression fails.
 return failures?1:0;
}
