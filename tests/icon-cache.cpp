// SPDX-License-Identifier: MIT
#include "IconCache.h"
#include <Application.h>
#include <iostream>
#include <vector>
int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-IconCacheTest");
  IconCache cache;std::vector<uint8> pixels(96*96*4,64);
  int checks=0,failures=0;
  auto check=[&](bool ok,const char *s){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<s<<std::endl;};
  auto first=cache.Get(pixels.data(),pixels.size(),96);
  check(bool(first),"native bitmap allocated");
  std::vector<std::shared_ptr<BBitmap>> entries;
  for(int i=0;i<2000;++i)entries.push_back(cache.Get(pixels.data(),pixels.size(),96));
  check(entries.back()==first && first.use_count()==2001,"2000 equal icons share native allocation");
  pixels[1]=65;auto custom=cache.Get(pixels.data(),pixels.size(),96);
  check(custom!=first && static_cast<uint8*>(custom->Bits())[1]==65,"custom icon pixels remain distinct");
  check(!cache.Get(pixels.data(),3,96),"malformed payload rejected");
  std::weak_ptr<BBitmap> weak=first;entries.clear();first.reset();
  check(weak.expired(),"index does not retain released listing icons");
  cache.Clear();check(static_cast<uint8*>(custom->Bits())[1]==65,"clearing index preserves live bitmap users");
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
