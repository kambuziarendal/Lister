// SPDX-License-Identifier: MIT
// Optional integration lane. Requires task-owned SMB/FTP/SFTP fixtures.
#include "FolderCompare.h"
#include <Application.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
int main(){
 BApplication app("application/x-vnd.Listi-compare-network");using namespace Listi;
 char pattern[]="/tmp/Listi-compare-network-XXXXXX";auto made=mkdtemp(pattern);if(!made)return 2;std::string local=made;
 mkdir((local+"/folder").c_str(),0700);
 std::ofstream(local+"/hello world.txt")<<"Listi network fixture\n";std::ofstream(local+"/Årsrapport.txt")<<"UTF-8 test\n";std::ofstream(local+"/folder/nested.txt")<<"nested\n";
 int failed=0;
 for(auto url:{"smb://127.0.0.1:1545/files","ftp://127.0.0.1:2221/","sftp://127.0.0.1:2322/"}){
  Connection c;c.username="listitest";c.password="fixture-only";c.knownHosts="/boot/home/listi-compare-known-hosts";SetConnection(url,c);
  auto result=CompareFolders({local,url,true});bool ok=result.complete&&result.different==0&&result.verified==3;
  std::cout<<(ok?"PASS ":"FAIL ")<<url<<" SHA-256 identical tree: "<<result.error<<std::endl;failed+=!ok;
  for(auto&e:result.entries)if(!CompareMatch(e.state))std::cout<<e.relative<<" "<<CompareLabel(e.state)<<" "<<e.detail<<std::endl;
  std::ofstream(local+"/hello world.txt")<<"Xisti network fixture\n";
  auto changed=CompareFolders({local,url,true});ok=changed.complete&&changed.different==1;std::cout<<(ok?"PASS ":"FAIL ")<<url<<" same-size content mismatch"<<std::endl;failed+=!ok;
  std::ofstream(local+"/hello world.txt")<<"Listi network fixture\n";
 }
 for(auto p:{"/hello world.txt","/Årsrapport.txt","/folder/nested.txt"})unlink((local+p).c_str());rmdir((local+"/folder").c_str());rmdir(local.c_str());return failed?1:0;
}
