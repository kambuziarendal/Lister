// SPDX-License-Identifier: MIT
// Synthetic native receiver for verifying exact B_REFS_RECEIVED delivery.
#include <Application.h>
#include <Entry.h>
#include <Path.h>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
class Receiver : public BApplication {
public:
  Receiver() : BApplication("application/x-vnd.Kambuzi-OpenWithFixture") {}
  void RefsReceived(BMessage *message) override {
    entry_ref ref;
    for(int32 i=0;message->FindRef("refs",i,&ref)==B_OK;++i) {
      BPath path(&ref);
      std::ofstream receipt("/boot/home/Listi-open-with-fixture/received",std::ios::app);
      receipt << (path.InitCheck()==B_OK?path.Path():"INVALID") << '\n';
    }
  }
};
int main(){
 int null=open("/dev/null",O_RDWR);
 if(null>=0){for(int fd=0;fd<3;++fd)dup2(null,fd);if(null>2)close(null);}
 Receiver app;app.Run();
}
