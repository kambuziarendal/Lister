// SPDX-License-Identifier: MIT
#include "OpenWith.h"
#include <Application.h>
#include <NodeInfo.h>
#include <MimeType.h>
#include <Roster.h>
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
// These checks register a unique disposable MIME type, never the preference of
// a real document type. Remove only the type successfully created by this run.
void CheckTypeDefault(const std::string &base, const OpenApplication &receiver) {
 const std::string type = "application/x-listi-default-" + std::to_string(getpid());
 BMimeType mime(type.c_str());
 Check(!mime.IsInstalled() && mime.Install() == B_OK, "register disposable type");
 struct Cleanup { BMimeType &mime; ~Cleanup() { mime.Delete(); } } cleanup{mime};
 const auto first = base + "/first", second = base + "/second";
 { std::ofstream(first) << "one"; std::ofstream(second) << "two"; }
 BNode a(first.c_str()), b(second.c_str());
 BNodeInfo one(&a), two(&b);
 Check(one.SetType(type.c_str()) == B_OK && two.SetType(type.c_str()) == B_OK,
       "both documents have the same private type");
 const auto editor = InspectApplication("/boot/system/apps/StyledEdit");
 Check(mime.SetPreferredApp(editor.signature.c_str()) == B_OK &&
       one.SetPreferredApp(editor.signature.c_str()) == B_OK, "seed type and file preferences");
 OpenWithSession session(first);
 auto info = session.Inspect();
 Check(info.CanSetTypeDefault() && info.filePreferred == editor.signature, "show file override");
 Refuse([&]{session.SetTypeDefault(receiver, "text/plain");}, "refuse a different displayed type");
 auto stale = receiver; ++stale.identity.st_ino;
 Refuse([&]{session.SetTypeDefault(stale, type);}, "refuse replaced application");
 char preference[B_MIME_TYPE_LENGTH] = {};
 Check(mime.GetPreferredApp(preference) == B_OK && preference == editor.signature,
       "rejected changes preserve old default");
 session.SetTypeDefault(receiver, type);
 auto updated = OpenWithSession(first).Inspect();
 Check(updated.typePreferred == receiver.signature && updated.preferred == editor.signature,
       "type default changes without overwriting selected file override");
 Check(OpenWithSession(second).Inspect().preferred == receiver.signature,
       "a second file inherits the persisted default");
 entry_ref ref, app;
 Check(get_ref_for_path(first.c_str(), &ref) == B_OK &&
       get_ref_for_path(editor.path.c_str(), &app) == B_OK, "resolve overridden document");
 entry_ref resolved;
 Check(be_roster->FindApp(&ref, &resolved) == B_OK && resolved == app,
       "normal native lookup still honors an explicit file preference");
 Check(get_ref_for_path(second.c_str(), &ref) == B_OK, "resolve sibling document");
 const std::string receipt = "/boot/home/Listi-open-with-fixture/received";
 const auto before = Contents(receipt);
 const auto status = be_roster->Launch(&ref);
 Check(status == B_OK || status == B_ALREADY_RUNNING, "ordinary system open uses file reference");
 for (int i = 0; i < 100 && Contents(receipt) == before; ++i) usleep(20000);
 Check(Contents(receipt) == before + second + "\n", "ordinary open reached the new default");
 Check(two.SetType("application/octet-stream") == B_OK, "create generic fixture");
 OpenWithSession generic(second);
 Check(!generic.Inspect().CanSetTypeDefault(), "generic file cannot set a system-wide default");
 Refuse([&]{generic.SetTypeDefault(receiver, "application/octet-stream");}, "worker enforces generic type restriction");
}
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
  const char *foldedSignature = "application/x-vnd.kambuzi-openwithfixture";
  Check(info.SetPreferredApp(foldedSignature) == B_OK, "store a case-variant native signature");
  const auto folded = OpenWithSession(path).Inspect();
  Check(!folded.applications.empty() && folded.applications.front().signature == receiver.signature,
        "native signature case does not hide the preferred application");
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
  CheckTypeDefault(base, receiver);
  std::cout<<passed<<" open-with checks passed"<<std::endl;
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
