// SPDX-License-Identifier: MIT
#include "FileProperties.h"
#include <Application.h>
#include <File.h>
#include <Message.h>
#include <fs_attr.h>
#include <functional>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
using namespace Lister;
int passed=0;
void Check(bool result,const char *label){if(!result)throw std::runtime_error(label);++passed;std::cout<<"PASS "<<label<<std::endl;}
void Refuse(std::function<void()> action,const char *label){bool refused=false;try{action();}catch(const std::exception &){refused=true;}Check(refused,label);}
FileAttribute Get(PropertySession &s,const std::string &name){for(auto a:s.Read().attributes)if(a.name==name)return a;throw std::runtime_error("Missing attribute: "+name);}
int main(){
 BApplication app("application/x-vnd.Kambuzi-PropertiesTest");
 try{
 char pattern[]="/boot/home/listi-properties-tests-XXXXXX";char *dir=mkdtemp(pattern);if(!dir)throw std::runtime_error("fixture");
 std::string root=dir,path=root+"/original";{std::ofstream f(path);f<<"original file bytes";}chmod(path.c_str(),0644);
 PropertySession session(path);BNode raw(path.c_str());
 Check(session.Read().writableKind && session.Read().stat.st_size==19,"inspect local file without changing bytes");
 session.Save(nullptr,"test:text",B_STRING_TYPE,"Grønn Å\nSecond line");auto text=Get(session,"test:text");
 Check(text.editable && text.text=="Grønn Å\nSecond line","Unicode multiline string roundtrip");
 session.Save(&text,text.name,text.type,"x");text=Get(session,text.name);Check(text.bytes.size()==2 && text.text=="x","shorter string truncates old bytes");
 session.Save(&text,text.name,text.type,"");text=Get(session,text.name);Check(text.editable && text.bytes.size()==1 && text.text.empty(),"empty string remains editable");
 for(auto item:std::vector<std::pair<type_code,std::string>>{{B_BOOL_TYPE,"true"},{B_INT32_TYPE,"-2147483648"},{B_UINT32_TYPE,"4294967295"},{B_INT64_TYPE,"-9223372036854775808"},{B_UINT64_TYPE,"18446744073709551615"},{B_FLOAT_TYPE,"1.25"},{B_DOUBLE_TYPE,"-2.5"}}){
   std::string name="test:"+AttributeTypeName(item.first);session.Save(nullptr,name,item.first,item.second);auto a=Get(session,name);Check(a.editable && a.text==item.second,"typed attribute exact roundtrip");
 }
 for(auto item:std::vector<std::pair<type_code,std::string>>{{B_BOOL_TYPE,"yes"},{B_INT32_TYPE,"2147483648"},{B_INT64_TYPE,"9223372036854775808"},{B_UINT32_TYPE,"4294967296"},{B_UINT64_TYPE,"18446744073709551616"},{B_UINT32_TYPE,"-1"},{B_FLOAT_TYPE,"nan"},{B_DOUBLE_TYPE,"inf"},{B_INT32_TYPE," 12"},{B_INT32_TYPE,"12x"},{B_FLOAT_TYPE,"1e100"}})
   Refuse([&]{EncodeAttribute(item.first,item.second);},"invalid or out-of-range typed value refused");
 Refuse([&]{EncodeAttribute(B_STRING_TYPE,std::string("\xc0\xaf",2));},"invalid UTF-8 input refused");
 Refuse([&]{EncodeAttribute(B_STRING_TYPE,std::string("a\0b",3));},"embedded null refused");
 Refuse([&]{session.Save(nullptr,text.name,B_STRING_TYPE,"collision");},"create never replaces existing attribute");
 raw.WriteAttr(text.name.c_str(),B_STRING_TYPE,0,"external",9);
 Refuse([&]{session.Save(&text,text.name,text.type,"overwrite");},"stale value rejected");
 Check(Get(session,text.name).text=="external","external value remains intact");
 text=Get(session,text.name);int32 number=12;raw.WriteAttr(text.name.c_str(),B_INT32_TYPE,0,&number,sizeof(number));
 Refuse([&]{session.Remove(text);},"stale type prevents removal");
 text=Get(session,text.name);Refuse([&]{session.Save(&text,text.name,B_STRING_TYPE,"12");},"existing attribute type cannot be changed");
 const char invalid[]={char(0xed),char(0xa0),char(0x80),0};raw.WriteAttr("test:malformed",B_STRING_TYPE,0,invalid,sizeof(invalid));
 Check(!Get(session,"test:malformed").editable,"malformed UTF-8 stored attribute is read-only");
 uint8 boolean=4;raw.WriteAttr("test:bad-bool",B_BOOL_TYPE,0,&boolean,1);Check(!Get(session,"test:bad-bool").editable,"malformed Boolean is read-only");
 raw.WriteAttr("BEOS:TYPE",B_STRING_TYPE,0,"text/plain",11);Check(!Get(session,"BEOS:TYPE").editable,"system MIME attribute inspection only");
 Refuse([&]{session.Save(nullptr,"_trk/private",B_STRING_TYPE,"x");},"native system namespace blocked");
 uint8 data[]={0,12,255};raw.WriteAttr("test:binary",B_RAW_TYPE,0,data,3);auto binary=Get(session,"test:binary");
 Check(!binary.editable && binary.bytes==std::vector<uint8>({0,12,255}),"unknown binary preserved for inspection");
 Refuse([&]{session.Remove(binary);},"binary removal blocked");
 std::vector<uint8> huge(65537,42);raw.WriteAttr("test:large",B_RAW_TYPE,0,huge.data(),huge.size());
 Check(!Get(session,"test:large").editable && Get(session,"test:large").size==65537,"oversized attribute is bounded and read-only");
 auto typed=Get(session,"test:Int32");session.Remove(typed);attr_info info;Check(raw.GetAttrInfo("test:Int32",&info)==B_ENTRY_NOT_FOUND,"explicit remove deletes only selected attribute");
 Check(Get(session,"test:UInt32").text=="4294967295","unrelated attributes preserved");
 session.Permissions(0644,"600");Check((session.Read().stat.st_mode&07777)==0600,"explicit permission change verified");
 Refuse([&]{session.Permissions(0644,"777");},"stale permissions rejected");
 Refuse([&]{session.Permissions(0600,"888");},"invalid octal rejected");
 Refuse([&]{session.Permissions(0600,"4755");},"new special permission bits rejected");
 chmod(path.c_str(),04600);Refuse([&]{session.Permissions(04600,"600");},"existing special permission bits preserved");chmod(path.c_str(),0600);
 std::string link=root+"/link";symlink(path.c_str(),link.c_str());PropertySession linked(link);
 Check(!linked.Read().writableKind && S_ISLNK(linked.Read().stat.st_mode),"symbolic link inspected without following target");
 Refuse([&]{linked.Save(nullptr,"test:link",B_STRING_TYPE,"no");},"symbolic link editing rejected");
 Check(raw.GetAttrInfo("test:link",&info)==B_ENTRY_NOT_FOUND,"link target untouched");
 std::string folder=root+"/folder";mkdir(folder.c_str(),0700);PropertySession directory(folder);directory.Save(nullptr,"test:folder",B_STRING_TYPE,"yes");Check(Get(directory,"test:folder").text=="yes","folder attributes supported");
 rename(path.c_str(),(root+"/moved").c_str());{std::ofstream f(path);f<<"replacement";}
 Refuse([&]{session.Save(nullptr,"test:retarget",B_STRING_TYPE,"no");},"replaced path cannot retarget pinned editor");
 BNode replacement(path.c_str());Check(replacement.GetAttrInfo("test:retarget",&info)==B_ENTRY_NOT_FOUND && raw.GetAttrInfo("test:retarget",&info)==B_ENTRY_NOT_FOUND,"neither replacement nor original mutated");
 std::ifstream original(root+"/moved");std::string content((std::istreambuf_iterator<char>(original)),{});Check(content=="original file bytes","file data unchanged by metadata operations");
 Refuse([&]{PropertySession unsupported("smb://host/share");},"virtual location rejected");
 Refuse([&]{directory.Save(nullptr,"bad\nname",B_STRING_TYPE,"x");},"control characters in attribute name refused");
 BNode folderNode(folder.c_str());
 for(int i=0;i<260;++i){std::string name="bounded:"+std::to_string(i);folderNode.WriteAttr(name.c_str(),B_INT32_TYPE,0,&i,sizeof(i));}
 auto bounded=directory.Read();Check(bounded.limited && bounded.attributes.size()==256,"attribute-count limit is explicit and bounded");
 std::string largeFolder=root+"/large-folder";mkdir(largeFolder.c_str(),0700);BNode largeNode(largeFolder.c_str());std::vector<uint8> block(65536,42);
 for(int i=0;i<20;++i){std::string name="large:"+std::to_string(i);largeNode.WriteAttr(name.c_str(),B_RAW_TYPE,0,block.data(),block.size());}
 PropertySession largeSession(largeFolder);bounded=largeSession.Read();Check(bounded.limited && bounded.attributes.size()==16,"total inspected attribute bytes bounded to one MiB");
 std::cout<<"TOTAL "<<passed<<" passed; fixture "<<root<<std::endl;
 return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL after "<<passed<<": "<<e.what()<<std::endl;return 1;}
}
