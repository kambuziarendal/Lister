// SPDX-License-Identifier: MIT
#include "OpenWith.h"
#include <Application.h>
#include <NodeInfo.h>
#include <MimeType.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace Listi;
int passed=0;
void Check(bool ok,const char *label){if(!ok)throw std::runtime_error(label);std::cout<<"PASS "<<label<<std::endl;++passed;}
void Refuse(std::function<void()> work,const char *label){bool denied=false;try{work();}catch(const std::exception &){denied=true;}Check(denied,label);}
std::string Contents(const std::string &path){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
int main(){
 BApplication app("application/x-vnd.Kambuzi-OpenWithTests");
 try {
  char pattern[]="/boot/home/listi-open-with-tests-XXXXXX";char *root=mkdtemp(pattern);if(!root)throw std::runtime_error("fixture");
  std::string base=root,path=base+"/Grønn '$(not-a-command)' file.txt";
  {std::ofstream file(path);file<<"original payload";}
  BNode node(path.c_str());BNodeInfo info(&node);char type[B_MIME_TYPE_LENGTH]={};
  node.RemoveAttr("BEOS:TYPE");node.RemoveAttr("BEOS:PREF_APP");
  OpenWithSession session(path);auto discovered=session.Inspect();
  Check(!discovered.mime.empty() && discovered.inferred,"untyped file gets read-only MIME detection");
  Check(info.GetType(type)!=B_OK,"detection does not write file MIME attribute");
  Check(info.GetPreferredApp(type)!=B_OK,"discovery does not write preferred application");
  Check(Contents(path)=="original payload","discovery preserves file contents");
  auto receiver=InspectApplication("/boot/home/Listi-open-with-fixture/Receiver");
  Check(receiver.signature=="application/x-vnd.Kambuzi-OpenWithFixture","native executable validated by app signature");
  std::string receipts="/boot/home/Listi-open-with-fixture/received",before=Contents(receipts);
  session.Launch(receiver);
  for(int i=0;i<100 && Contents(receipts)==before;++i)usleep(20000);
  Check(Contents(receipts)==before+path+"\n","cold launch delivers exact Unicode and shell-like pathname via refs");
  before=Contents(receipts);session.Launch(receiver);
  for(int i=0;i<100 && Contents(receipts)==before;++i)usleep(20000);
  Check(Contents(receipts)==before+path+"\n","already running application receives second file reference");
  Check(info.GetType(type)!=B_OK && info.GetPreferredApp(type)!=B_OK,"launch leaves associations untouched");
  info.SetType("text/plain");OpenWithSession typed(path);auto known=typed.Inspect();
  Check(known.mime=="text/plain" && !known.inferred,"stored MIME type takes precedence");
  Check(!known.applications.empty(),"native supporting application discovery works");
  info.SetPreferredApp(receiver.signature.c_str());OpenWithSession preferred(path);auto choices=preferred.Inspect();
  Check(choices.preferred==receiver.signature,"per-file preferred application is displayed");
  Check(!choices.applications.empty() && choices.applications[0].signature==receiver.signature,"preferred application ordered first");
  info.SetType("application/x-listi-unknown-fixture");info.SetPreferredApp("application/x-vnd.Listi-MissingFixture");
  OpenWithSession unknown(path);auto missing=unknown.Inspect();
  Check(missing.applications.empty(),"unknown type and missing preferred app allow explicit picker fallback");
  Refuse([&]{InspectApplication(path);},"non-executable document rejected as application");
  chmod(path.c_str(),0755);Refuse([&]{InspectApplication(path);},"executable text without native app signature rejected");chmod(path.c_str(),0644);
  Refuse([&]{InspectApplication("/boot/system/apps/Listi");},"recursive Listi launch refused");
  Refuse([&]{OpenWithSession folder(base);},"directory not treated as document");
  Refuse([&]{OpenWithSession remote("smb://example/share/file");},"remote file requires explicit local copy");
  std::string link=base+"/link";symlink(path.c_str(),link.c_str());
  Refuse([&]{OpenWithSession linked(link);},"symbolic document link requires explicit target");
  OpenWithSession changed(path);{std::ofstream file(path,std::ios::app);file<<"changed";}
  Refuse([&]{changed.Launch(receiver);},"changed document is rejected before launch");
  OpenWithSession renamed(path);rename(path.c_str(),(path+".moved").c_str());{std::ofstream file(path);file<<"replacement";}
  before=Contents(receipts);Refuse([&]{renamed.Launch(receiver);},"replaced pathname does not launch replacement");
  Check(Contents(receipts)==before,"rejected source sends no application request");
  OpenWithSession valid(path);auto stale=receiver;stale.identity.st_ino++;
  Refuse([&]{valid.Launch(stale);},"changed application identity rejected");
  unlink(path.c_str());Refuse([&]{valid.Launch(receiver);},"deleted document rejected");
  Check(Contents(receipts)==before,"all rejected launches leave receiver unchanged");
  std::cout<<passed<<" open-with checks passed"<<std::endl;
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
