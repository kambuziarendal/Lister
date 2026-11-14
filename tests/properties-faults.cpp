// SPDX-License-Identifier: MIT
#include "FileProperties.h"
#include <Application.h>
#include <File.h>
#include <Message.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <unistd.h>
using namespace Lister;
static int fault=0,passed=0;
extern "C" ssize_t RealWrite(BNode*,const char*,type_code,off_t,const void*,size_t) asm("__real__ZN5BNode9WriteAttrEPKcjlPKvm");
extern "C" ssize_t WrappedWrite(BNode*,const char*,type_code,off_t,const void*,size_t) asm("__wrap__ZN5BNode9WriteAttrEPKcjlPKvm");
extern "C" ssize_t WrappedWrite(BNode *n,const char *k,type_code t,off_t p,const void *b,size_t s){if(fault==1){fault=0;return RealWrite(n,k,t,p,b,s-1);}return RealWrite(n,k,t,p,b,s);}
extern "C" status_t RealSync(BNode*) asm("__real__ZN5BNode4SyncEv");
extern "C" status_t WrappedSync(BNode*) asm("__wrap__ZN5BNode4SyncEv");
extern "C" status_t WrappedSync(BNode *n){if(fault==2){fault=0;return B_IO_ERROR;}return RealSync(n);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(fault==3){fault=0;errno=EIO;return -1;}return __real_fsync(fd);}
void Check(bool ok,const char *s){if(!ok)throw std::runtime_error(s);std::cout<<"PASS "<<s<<std::endl;++passed;}
FileAttribute Get(PropertySession &s){for(auto a:s.Read().attributes)if(a.name=="test:original")return a;throw std::runtime_error("missing");}
int main(){BApplication app("application/x-vnd.Kambuzi-PropertiesFaults");try{
 char p[]="/boot/home/listi-properties-faults-XXXXXX";int fd=mkstemp(p);if(fd<0)return 1;write(fd,"payload",7);close(fd);
 PropertySession session(p);
 for(int injected=1;injected<=3;++injected){
   BNode raw(p);raw.WriteAttr("test:original",B_STRING_TYPE,0,"original metadata",18);auto before=Get(session);fault=injected;
   std::string error;try{session.Save(&before,before.name,before.type,"new metadata");}catch(const std::exception &e){error=e.what();}
   Check(!error.empty() && fault==0,"injected metadata failure reported");
   auto pos=error.find("/boot/home/");Check(pos!=std::string::npos,"failure identifies retained recovery record");
   std::string recovery=error.substr(pos);BFile backup(recovery.c_str(),B_READ_ONLY);BMessage record;
   if(injected<3){Check(record.Unflatten(&backup)==B_OK,"recovery record readable after mutation failure");const void *data;ssize_t size;Check(record.FindData("bytes",B_RAW_TYPE,&data,&size)==B_OK && size==18 && memcmp(data,"original metadata",18)==0,"recovery retains exact original typed bytes");Check(record.GetInt32("type",0)==B_STRING_TYPE && record.GetBool("existed",false),"recovery retains original type and existence");}
   else Check(Get(session).bytes==before.bytes,"failed recovery flush prevents attribute mutation");
   unlink(recovery.c_str());
 }
 std::ifstream f(p);std::string content((std::istreambuf_iterator<char>(f)),{});Check(content=="payload","all injected failures preserve file contents");unlink(p);
 std::cout<<"TOTAL "<<passed<<" passed"<<std::endl;return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
