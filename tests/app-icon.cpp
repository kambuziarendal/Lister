// SPDX-License-Identifier: MIT
#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <IconUtils.h>
#include <NodeInfo.h>
#include <Resources.h>
#include <TranslatorRoster.h>
#include <TranslationDefs.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
int main(int argc,char **argv) {
  if (argc != 3) return 2;
  BApplication app("application/x-vnd.Kambuzi-ListiIconTest");
  BFile executable(argv[1],B_READ_ONLY); BResources resources(&executable);
  size_t size = 0;
  auto data = static_cast<const uint8 *>(resources.LoadResource('VICN',"BEOS:ICON",&size));
  if (!data || size < 10 || memcmp(data,"ncif",4)) return 3;
  printf("PASS native HVIF resource present (%zu bytes)\n",size);
  for (int pixels : {16,32,64,128}) {
    auto bitmap = new BBitmap(BRect(0,0,pixels-1,pixels-1),B_RGBA32);
    if (BIconUtils::GetVectorIcon(data,size,bitmap) != B_OK) return 4;
    const auto bits = static_cast<uint8 *>(bitmap->Bits()); int painted = 0;
    for (int y=0;y<pixels;++y) for (int x=0;x<pixels;++x)
      if (bits[y*bitmap->BytesPerRow()+4*x+3]) ++painted;
    if (painted < pixels*pixels/5 || painted > pixels*pixels*9/10) return 5;
    if (pixels == 16 || pixels == 32) {
      BBitmap tracker(BRect(0,0,pixels-1,pixels-1),B_RGBA32);
      BNodeInfo info(&executable);
      if (info.GetTrackerIcon(&tracker,static_cast<icon_size>(pixels)) != B_OK) return 6;
      printf("PASS Tracker resolves %dpx application icon\n",pixels);
    }
    BBitmapStream stream(bitmap);
    std::string path = std::string(argv[2])+"/icon-"+std::to_string(pixels)+".png";
    BFile output(path.c_str(),B_WRITE_ONLY|B_CREATE_FILE|B_ERASE_FILE);
    if (BTranslatorRoster::Default()->Translate(&stream,nullptr,nullptr,&output,B_PNG_FORMAT) != B_OK) return 7;
    printf("PASS native render %dpx with transparent margin\n",pixels);
  }
  return 0;
}
