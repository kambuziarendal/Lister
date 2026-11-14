// SPDX-License-Identifier: MIT
#include "Common.h"
#include <Application.h>
#include <cmath>
#include <iostream>
#include <limits>
int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-PreferencesTest");
  int checks=0, failures=0;
  auto check=[&](bool ok,const char *label) {
    ++checks;failures+=!ok;
    std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;
  };
  Preferences live;
  auto stale=live.Save();
  stale.ReplaceBool("icons",false);stale.ReplaceFloat("fontSize",17);
  live.paneMode[0]=3;live.paneMode[1]=2;
  live.paneTree[0]=true;live.paneTree[1]=true;
  live.preview=true;live.folderModes["/synthetic"]=1;
  live.ApplyDialog(stale);
  check(!live.icons && live.font.Size()==17,"dialog appearance fields apply");
  check(live.paneMode[0]==3 && live.paneMode[1]==2,"stale dialog preserves current pane modes");
  check(live.paneTree[0] && live.paneTree[1],"stale dialog preserves both trees");
  check(live.preview && live.folderModes["/synthetic"]==1,"stale dialog preserves preview and folder overrides");
  for(float size:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-1.f,7.f,41.f}) {
    BMessage m;m.AddFloat("fontSize",size);live.Load(m);
    check(live.font.Size()==12,"invalid persisted font safely defaults to 12");
  }
  for(float size:{8.f,12.f,40.f}) {
    BMessage m;m.AddFloat("fontSize",size);live.Load(m);
    check(live.font.Size()==size,"valid font boundaries preserved");
  }
  std::cout<<checks<<" checks, "<<failures<<" failures\n";
  return failures?1:0;
}
