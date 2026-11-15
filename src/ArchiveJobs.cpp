// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "ArchiveBackend.h"
#include "ArchiveVolumes.h"
#include "DynamicLibrary.h"
#include "JobSnapshot.h"
#include "LocalMoveTree.h"
#include "VirtualFile.h"
#include <Directory.h>
#include <Entry.h>
#include <Node.h>
#include <Path.h>
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <set>
#include <fs_attr.h>

namespace Lister {
namespace {
void Require(bool ok, const std::string &message) { if (!ok) throw std::runtime_error(message); }
struct FD { int n; explicit FD(int value):n(value){} ~FD(){if(n>=0)close(n);} };
struct Writer {
  DynamicLibrary lib{"libarchive.so", "libarchive.so.13"};
  archive *a = nullptr;
  Writer() {
    Require(lib.Available(), "Install the optional libarchive package from HaikuDepot, then try again.");
    a = LIBCALL(lib, archive_write_new)(); Require(a, "Cannot allocate archive writer.");
  }
  ~Writer(){ if(a) LIBCALL(lib, archive_write_free)(a); }
  void Check(int result) {
    if(result == ARCHIVE_OK) return;
    const char *error=LIBCALL(lib, archive_error_string)(a);
    throw std::runtime_error(error ? error : "Archive writing failed.");
  }
  void Configure(const std::string &format, int level,
                 const ArchiveSecretPtr &secret = {}) {
    Require(level>=0 && level<=9, "Invalid compression level.");
    if(format=="zip") {
      Check(LIBCALL(lib, archive_write_set_format_zip)(a));
      if(secret) {
        Check(LIBCALL(lib, archive_write_set_passphrase)(a,secret->Text()));
        Check(LIBCALL(lib, archive_write_set_options)(a,"zip:encryption=aes256"));
      }
    }
    else if(format=="7z") Check(LIBCALL(lib, archive_write_set_format_7zip)(a));
    else {
      if (format == "cpio") Check(LIBCALL(lib, archive_write_set_format_cpio_newc)(a));
      else if (format == "xar") Check(LIBCALL(lib, archive_write_set_format_xar)(a));
      else {
        Require(format=="tar" || format=="tgz" || format=="txz" || format=="tbz2" ||
          format=="tlz" || format=="tzst" || format=="tlz4" || format=="tlzip", "Unknown archive format.");
        Check(LIBCALL(lib, archive_write_set_format_pax_restricted)(a));
        const char *filter = format=="tgz" ? "gzip" : format=="txz" ? "xz" :
          format=="tbz2" ? "bzip2" : format=="tlz" ? "lzma" : format=="tzst" ? "zstd" :
          format=="tlz4" ? "lz4" : format=="tlzip" ? "lzip" : "none";
        if (format != "tar") Check(LIBCALL(lib, archive_write_add_filter_by_name)(a, filter));
      }
    }
    if(format!="tar" && format!="cpio") {
      const std::string option="compression-level="+std::to_string(level);
      Check(LIBCALL(lib, archive_write_set_options)(a,option.c_str()));
    }
  }
};
const ArchiveFormat formats[]={{"zip","ZIP",".zip",true},{"7z","7z",".7z",true},
 {"tgz","Tar + gzip",".tar.gz",true},{"txz","Tar + xz",".tar.xz",true},
 {"tbz2","Tar + bzip2",".tar.bz2",true},{"tlz","Tar + LZMA",".tar.lzma",true},
 {"tzst","Tar + Zstandard",".tar.zst",true},{"tlz4","Tar + LZ4",".tar.lz4",true},
 {"tlzip","Tar + lzip",".tar.lz",true},{"tar","Tar (uncompressed)",".tar",false},
 {"cpio","CPIO (newc)",".cpio",false},{"xar","XAR",".xar",true}};
std::string LocalSource(const std::string &path) {
  auto loc=ParseLocation(path);
  Require(loc.Local() || loc.Archive(), "Archive jobs currently require local files or a local archive.");
  return loc.Archive()?loc.archive:path;
}
void VerifyArchiveSets(const Job &job) {
  for(const auto &item:job.expectedArchiveSets)
    Require(FindArchiveVolumes(item.first).Fingerprint()==item.second,"Archive parts changed after selection. Select the archive again.");
}
void Guards(const Job &job) {
  for(const auto &item:job.expectedSources) {
    struct stat s;
    Require(!lstat(item.first.c_str(),&s) && s.st_dev==item.second.device && s.st_ino==item.second.inode,
      "A selected source was replaced. Select it again.");
  }
  if(!job.destinationFolder.empty()) {
    struct stat s;
    Require(!stat(job.destinationFolder.c_str(),&s) && S_ISDIR(s.st_mode) &&
      s.st_dev==job.expectedDestination.device && s.st_ino==job.expectedDestination.inode,
      "The destination folder was replaced. Choose it again.");
  }
}
bool Unchanged(const struct stat &a,const struct stat &b) {
  return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_mode==b.st_mode &&
    a.st_size==b.st_size && a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec &&
    a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
std::string PathOf(BDirectory &dir) {
  BEntry e; BPath p;
  Require(dir.GetEntry(&e)==B_OK && e.GetPath(&p)==B_OK,"The working folder disappeared.");
  return p.Path();
}
void RemoveContents(BDirectory &dir) {
  // Extracted directory modes may be read-only. This is our private staging tree.
  dir.SetPermissions(0700);
  BEntry entry;
  while(dir.GetNextEntry(&entry,false)==B_OK) {
    struct stat s;
    if(entry.GetStat(&s)==B_OK && S_ISDIR(s.st_mode)) {
      BDirectory child(&entry); RemoveContents(child);
    }
    entry.Remove();
  }
}
struct Stage {
  BDirectory parent, dir;
  Stage(const std::string &folder):parent(folder.c_str()) {
    Require(parent.InitCheck()==B_OK,"Cannot open destination folder.");
    std::string pattern=Join(folder,".Listi-archive-stage-XXXXXX");
    std::vector<char> bytes(pattern.begin(),pattern.end()); bytes.push_back(0);
    char *made=mkdtemp(bytes.data()); Require(made,"Cannot create archive staging folder.");
    Require(dir.SetTo(made)==B_OK,"Cannot open archive staging folder.");
  }
  ~Stage(){RemoveContents(dir);BEntry e;if(dir.GetEntry(&e)==B_OK)e.Remove();}
};
struct Pack {
  Writer &writer; const Hooks &hooks; size_t count=0;
  std::vector<std::pair<std::string,struct stat>> snapshots;
  void Verify() {
    for(const auto &item:snapshots){Check();struct stat now;
      Require(!lstat(item.first.c_str(),&now)&&Unchanged(item.second,now),"Source changed before archive completion: "+item.first);}
  }
  std::vector<char> buffer=std::vector<char>(131072);
  void Check() {if(hooks.proceed && !hooks.proceed())throw Cancelled();}
  void Add(const std::string &path,const std::string &name,unsigned depth=0) {
    Check(); Require(depth<=64 && ++count<=200000,"Archive exceeds the folder or entry limit.");
    Require(!name.empty() && name.find_first_of("\r\n\\")==std::string::npos &&
      !(depth==0 && name.size()>1 && name[1]==':'),"Unsupported archive entry name.");
    struct stat before,after;
    Require(!lstat(path.c_str(),&before),"Cannot read source: "+path);
    Require(S_ISREG(before.st_mode)||S_ISDIR(before.st_mode),
      "Links and special files are not packed in this version: "+path);
    snapshots.push_back({path,before});
    FD fd(open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK));
    Require(fd.n>=0 && !fstat(fd.n,&after) && Unchanged(before,after),"Source changed before packing: "+path);
    auto entry=LIBCALL(writer.lib,archive_entry_new)(); Require(entry,"Cannot allocate archive entry.");
    try {
      LIBCALL(writer.lib,archive_entry_copy_stat)(entry,&before);
      LIBCALL(writer.lib,archive_entry_set_pathname)(entry,name.c_str());
      LIBCALL(writer.lib,archive_entry_set_perm)(entry,before.st_mode&0777);
      LIBCALL(writer.lib,archive_entry_set_uid)(entry,0);LIBCALL(writer.lib,archive_entry_set_gid)(entry,0);
      if(S_ISDIR(before.st_mode))LIBCALL(writer.lib,archive_entry_set_size)(entry,0);
      writer.Check(LIBCALL(writer.lib,archive_write_header)(writer.a,entry));
      LIBCALL(writer.lib,archive_entry_free)(entry);entry=nullptr;
      if(S_ISREG(before.st_mode)) {
        off_t total=0;
        for(;;) {
          Check();ssize_t n=read(fd.n,buffer.data(),buffer.size());
          if(n<0 && errno==EINTR)continue;
          Require(n>=0,"Cannot read source: "+path);if(!n)break;
          total+=n;Require(total<=before.st_size,"Source grew during packing: "+path);
          size_t offset=0;
          while(offset<size_t(n)) {
            auto written=LIBCALL(writer.lib,archive_write_data)(writer.a,buffer.data()+offset,n-offset);
            Require(written>0,"Cannot write archive data.");offset+=written;
          }
          if(hooks.progress)hooks.progress(path,n);
        }
        Require(total==before.st_size,"Source size changed during packing: "+path);
      }
      writer.Check(LIBCALL(writer.lib,archive_write_finish_entry)(writer.a));
      if(S_ISDIR(before.st_mode)) {
        DIR *raw=fdopendir(dup(fd.n));Require(raw,"Cannot list source folder: "+path);
        std::unique_ptr<DIR,int(*)(DIR*)> dir(raw,closedir);
        for(;;) {
          errno=0;auto child=readdir(raw);
          if(!child){Require(errno==0,"Cannot finish reading folder: "+path);break;}
          if(!strcmp(child->d_name,".")||!strcmp(child->d_name,".."))continue;
          Add(Join(path,child->d_name),Join(name,child->d_name),depth+1);
        }
      }
      Require(!fstat(fd.n,&after) && Unchanged(before,after),"Source changed during packing: "+path);
      Require(!lstat(path.c_str(),&after) && Unchanged(before,after),"Source was replaced during packing: "+path);
    } catch(...) {if(entry)LIBCALL(writer.lib,archive_entry_free)(entry);throw;}
  }
};
std::vector<std::string> Children(const std::string &folder) {
  DIR *raw=opendir(folder.c_str());Require(raw,"Cannot list archive staging folder.");
  std::unique_ptr<DIR,int(*)(DIR*)> dir(raw,closedir);
  std::vector<std::string> result;
  for(;;){errno=0;auto child=readdir(raw);if(!child){Require(!errno,"Cannot finish listing archive staging folder.");break;}
    if(!strcmp(child->d_name,".")||!strcmp(child->d_name,".."))continue;
    result.push_back(Join(folder,child->d_name));}
  std::sort(result.begin(),result.end());return result;
}
void RemoveTree(const std::string &path,const Hooks &hooks,unsigned depth=0) {
  if(hooks.proceed&&!hooks.proceed())throw Cancelled();
  Require(depth<=64,"Archive edit exceeds the folder depth limit.");
  struct stat st;Require(!lstat(path.c_str(),&st),"Archive entry selected for removal disappeared.");
  Require(!S_ISLNK(st.st_mode),"Archive edit staging unexpectedly contains a link.");
  if(S_ISDIR(st.st_mode))for(const auto &child:Children(path))RemoveTree(child,hooks,depth+1);
  Require(!(S_ISDIR(st.st_mode)?rmdir(path.c_str()):unlink(path.c_str())),"Cannot remove an entry from archive staging.");
}
void CopyMetadata(const std::string &fromPath,const std::string &toPath,const Hooks &hooks) {
  struct stat st;Require(!stat(fromPath.c_str(),&st),"Cannot inspect archive metadata.");
  Require(!chmod(toPath.c_str(),st.st_mode&07777),"Cannot preserve archive permissions.");
  BNode from(fromPath.c_str()),to(toPath.c_str());
  Require(from.InitCheck()==B_OK&&to.InitCheck()==B_OK,"Cannot open archive metadata.");
  char name[B_ATTR_NAME_LENGTH];std::vector<char> bytes(65536);status_t status;
  while((status=from.GetNextAttrName(name))==B_OK){if(hooks.proceed&&!hooks.proceed())throw Cancelled();
    attr_info info;Require(from.GetAttrInfo(name,&info)==B_OK,"Archive metadata changed during editing.");
    if(!info.size){Require(to.WriteAttr(name,info.type,0,nullptr,0)==0,"Cannot preserve empty archive metadata.");continue;}
    off_t at=0;while(at<info.size){auto amount=std::min<off_t>(bytes.size(),info.size-at);
      auto got=from.ReadAttr(name,info.type,at,bytes.data(),amount);Require(got>0,"Cannot read archive metadata.");
      Require(to.WriteAttr(name,info.type,at,bytes.data(),got)==got,"Cannot preserve archive metadata.");at+=got;}}
  Require(status==B_ENTRY_NOT_FOUND,"Cannot enumerate archive metadata.");
}
void CreateOutput(int output,const std::string &format,const std::vector<std::string> &sources,
                  int level,bool encryptNames,const ArchiveSecretPtr &secret,const Hooks &hooks) {
  if(secret&&format=="7z")SevenZipCreate(output,sources,level,encryptNames,secret,hooks);
  else {
    Writer writer;writer.Configure(format,level,secret);
    writer.Check(LIBCALL(writer.lib,archive_write_open_fd)(writer.a,output));
    Pack pack{writer,hooks};for(const auto &source:sources)pack.Add(source,Leaf(source));
    writer.Check(LIBCALL(writer.lib,archive_write_close)(writer.a));pack.Verify();
  }
}
std::string EditFormat(const std::string &path) {
  const auto parts=FindArchiveVolumes(path);
  Require(parts.kind==ArchiveVolumes::Single,"Split archives cannot be edited. Extract them, change the files, and create a new archive.");
  if(SevenZipFormat(path)=="7z")return "7z";
  FD fd(open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK));Require(fd.n>=0,"Cannot open archive for editing.");
  unsigned char magic[4]{};auto got=read(fd.n,magic,sizeof(magic));
  if(got>=4&&magic[0]=='P'&&magic[1]=='K'&&
     ((magic[2]==3&&magic[3]==4)||(magic[2]==5&&magic[3]==6)||(magic[2]==7&&magic[3]==8)))return "zip";
  Require(SevenZipFormat(path).empty(),
    "This archive format has no installed writer. It can be browsed and extracted, but cannot be changed.");
  DynamicLibrary lib{"libarchive.so", "libarchive.so.13"};
  Require(lib.Available(), "Install libarchive to edit this archive format.");
  struct Reader {
    DynamicLibrary &lib; archive *value;
    ~Reader() { if (value) LIBCALL(lib, archive_read_free)(value); }
  } reader{lib, LIBCALL(lib, archive_read_new)()};
  Require(reader.value, "Cannot inspect archive format.");
  LIBCALL(lib, archive_read_support_format_all)(reader.value);
  LIBCALL(lib, archive_read_support_filter_all)(reader.value);
  Require(lseek(fd.n,0,SEEK_SET)==0 &&
    LIBCALL(lib, archive_read_open_fd)(reader.value,fd.n,65536)==ARCHIVE_OK,
    "Cannot inspect this archive for editing.");
  archive_entry *entry=nullptr;
  const int status=LIBCALL(lib, archive_read_next_header)(reader.value,&entry);
  Require(status==ARCHIVE_OK || status==ARCHIVE_EOF, "Archive is damaged or cannot be inspected for editing.");
  const int detected=LIBCALL(lib, archive_format)(reader.value)&ARCHIVE_FORMAT_BASE_MASK;
  std::string format=detected==ARCHIVE_FORMAT_TAR ? "tar" :
    detected==ARCHIVE_FORMAT_CPIO ? "cpio" : detected==ARCHIVE_FORMAT_XAR ? "xar" : "";
  Require(!format.empty(), "This archive type has no qualified writer in Listi. It can be browsed and extracted; the original is unchanged.");
  for (int i=0;i<LIBCALL(lib, archive_filter_count)(reader.value);++i) {
    const int filter=LIBCALL(lib, archive_filter_code)(reader.value,i);
    if (filter==ARCHIVE_FILTER_NONE) continue;
    Require(format=="tar", "This archive compression wrapper cannot be preserved when editing.");
    switch(filter) {
      case ARCHIVE_FILTER_GZIP: format="tgz"; break;
      case ARCHIVE_FILTER_BZIP2: format="tbz2"; break;
      case ARCHIVE_FILTER_XZ: format="txz"; break;
      case ARCHIVE_FILTER_LZMA: format="tlz"; break;
      case ARCHIVE_FILTER_ZSTD: format="tzst"; break;
      case ARCHIVE_FILTER_LZ4: format="tlz4"; break;
      case ARCHIVE_FILTER_LZIP: format="tlzip"; break;
      default: throw std::runtime_error("This archive compression wrapper has no qualified writer. The original is unchanged.");
    }
  }
  Writer probe;probe.Configure(format,6);
  return format;
}
std::string MemberPath(const std::string &path) {
  auto value=path;while(!value.empty()&&value.front()=='/')value.erase(value.begin());
  if(value.empty())return "";
  return SafeArchivePath(value.c_str());
}
void VerifyMovedEntry(const std::string &, const std::string &, const TransferControl &, unsigned depth = 0);
Result RunArchiveEdit(const Job &job,const Hooks &hooks) {
  Result result;
  try {
    Require(!job.destination.empty()&&!job.destinationFolder.empty(),"Archive edit target is missing.");
    Require(!job.sources.empty()||!job.archiveRemove.empty()||!job.archiveFolders.empty()||!job.archiveRenames.empty(),"Choose an archive change first.");
    Guards(job);VerifyArchiveSets(job);
    const auto format=EditFormat(job.destination);
    Require(format==job.archiveFormat,"Archive format changed after selection.");
    const auto passwordGeneration=ArchivePasswordGeneration();
    Stage stage(job.destinationFolder);const auto work=Join(PathOf(stage.dir),"contents");
    Require(!mkdir(work.c_str(),0700),"Cannot create archive edit staging folder.");
    auto check=[&]{if(hooks.proceed&&!hooks.proceed())throw Cancelled();
      CheckArchivePasswordGeneration(passwordGeneration);Guards(job);VerifyArchiveSets(job);};
    TransferControl control;control.proceed=[&]{check();return true;};
    control.progress=[&](uint64_t bytes){if(hooks.progress)hooks.progress(job.destination,bytes);};
    Hooks internal=hooks;internal.fileDone={};internal.fileFailed={};
    internal.proceed=[&]{check();return true;};
    internal.progress=[&](const std::string &,uint64_t bytes){
      if(hooks.progress)hooks.progress(job.destination,bytes);};
    check();const auto root=OpenArchive(job.destination,control);Require(!root.empty(),"The selected file is not a supported archive.");
    auto top=ListFiles(root,control);
    if(!top.empty()){
      Job extract;extract.action=Action::Copy;extract.destination=work;
      for(const auto &entry:top)extract.sources.push_back(entry.path);
      auto extraction=internal;extraction.conflict={};
      auto copied=Run(extract,extraction);if(copied.cancelled)throw Cancelled();
      Require(copied.errors.empty(),copied.errors.empty()?"Archive extraction failed.":copied.errors.front());
      Require(!copied.skipped,"Archive contains conflicting entries and was not changed.");
    }
    for(const auto &member:job.archiveRemove){check();const auto relative=MemberPath(member);
      Require(!relative.empty(),"The archive root cannot be removed.");
      const auto target=Join(work,relative);struct stat st;
      Require(!lstat(target.c_str(),&st),"An archive entry selected for removal no longer exists.");
      RemoveTree(target,internal);
    }
    for (const auto &path : job.archiveFolders) {
      check();const auto relative=MemberPath(path);
      Require(!relative.empty(), "The archive root already exists.");
      Require(!mkdir(Join(work,relative).c_str(),0755), "Could not create archive folder. The name may already exist.");
    }
    for (const auto &change : job.archiveRenames) {
      check();const auto from=MemberPath(change.first),to=MemberPath(change.second);
      Require(!from.empty()&&!to.empty()&&to!=from&&to.compare(0,from.size()+1,from+"/")!=0,
        "An archive entry cannot be moved onto itself or into its contents.");
      BEntry entry(Join(work,from).c_str(),false);
      BDirectory parent(ParentLocation(Join(work,to)).c_str());
      Require(entry.MoveTo(&parent,Leaf(to).c_str(),false)==B_OK,
        "Could not rename or move the archive entry. Existing entries were not replaced.");
    }
    if(!job.sources.empty()){
      const auto relative=MemberPath(job.archivePath),target=relative.empty()?work:Join(work,relative);
      struct stat targetStat;Require(!stat(target.c_str(),&targetStat)&&S_ISDIR(targetStat.st_mode),
        "The destination folder inside the archive no longer exists.");
      Job add;add.action=Action::Copy;add.destination=target;
      add.destinationFolder=target;add.expectedDestination={int32_t(targetStat.st_dev),int64_t(targetStat.st_ino)};
      for(const auto &source:job.sources) {
        auto location=ParseLocation(source);
        if(location.Archive()&&location.archive==job.destination) {
          const auto relative=MemberPath(location.path);
          Require(!relative.empty(), "An archive cannot be added to itself.");
          const auto staged=Join(work,relative);struct stat st;
          Require(!lstat(staged.c_str(),&st), "The source archive entry disappeared.");
          add.sources.push_back(staged);
          add.expectedSources[staged]={int32_t(st.st_dev),int64_t(st.st_ino)};
        } else {
          add.sources.push_back(source);
          if(!IsVirtual(source))add.expectedSources[source]=job.expectedSources.at(source);
        }
      }
      auto adding=internal;
      adding.conflict=[&](const std::string &destination){
        if(!hooks.conflict)return Decision::Abort;
        auto relative=destination.size()>work.size()&&destination.compare(0,work.size(),work)==0
          ?destination.substr(work.size()):std::string("/")+Leaf(destination);
        if(relative.empty()||relative.front()!='/')relative="/"+relative;
        return hooks.conflict(job.destination+" → "+relative);
      };
      auto added=Run(add,adding);if(added.cancelled)throw Cancelled();
      Require(added.errors.empty(),added.errors.empty()?"Could not add files to archive.":added.errors.front());
      Require(!added.skipped,"Archive edit was cancelled at a name conflict.");
    }
    check();const auto outputPath=Join(PathOf(stage.dir),"updated");
    FD output(open(outputPath.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600));
    Require(output.n>=0,"Cannot create updated archive output.");
    auto secret=CachedArchivePassword(job.destination,
      FindArchiveVolumes(job.destination).Fingerprint());
    CreateOutput(output.n,format,Children(work),job.archiveLevel,
      secret&&format=="7z",secret,internal);
    Require(!fsync(output.n),"Cannot flush updated archive to disk.");
    CopyMetadata(job.destination,outputPath,internal);
    Require(!fsync(output.n),"Cannot flush updated archive metadata.");check();
    if (job.archiveVerifyOutput) {
      if (secret) {
        struct stat st; Require(!lstat(outputPath.c_str(), &st), "Updated archive disappeared before verification.");
        RememberArchivePassword(ArchivePasswordIdentity(st), secret, passwordGeneration);
        RememberArchivePassword(FindArchiveVolumes(outputPath).Fingerprint(), secret, passwordGeneration);
      }
      TransferControl verify; verify.proceed = internal.proceed;
      const auto root = OpenArchive(outputPath, verify);
      Require(!root.empty(), "Updated archive could not be reopened. Original archive was retained.");
      const auto children = Children(work);
      Require(ListFiles(root, verify).size() == children.size(), "Updated archive has a different member set. Original archive was retained.");
      for (const auto &child : children)
        VerifyMovedEntry(child, ChildLocation(root, Leaf(child)), verify);
      check();
    }
    struct stat original,named;FD originalFd(open(job.destination.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK));
    Require(originalFd.n>=0&&!fstat(originalFd.n,&original)&&!lstat(job.destination.c_str(),&named)&&Unchanged(original,named),
      "The archive was replaced while it was being edited. Nothing was changed.");
    Require(!rename(outputPath.c_str(),job.destination.c_str()),"Could not publish the updated archive. The original was left in place.");
    FD parent(open(job.destinationFolder.c_str(),O_RDONLY));Require(parent.n>=0&&!fsync(parent.n),"Could not flush the updated archive directory.");
    if(secret)try {struct stat published;
      if(!lstat(job.destination.c_str(),&published))
        RememberArchivePassword(ArchivePasswordIdentity(published),secret,passwordGeneration);
      RememberArchivePassword(FindArchiveVolumes(job.destination).Fingerprint(),secret,passwordGeneration);
    }catch(...){/* Password caching is optional after the archive is durable. */}
    result.completed=1;result.completedRoots.push_back({job.destination,job.destination});
    if(hooks.fileDone)hooks.fileDone(job.destination,job.destination);
  }catch(const Cancelled &){result.cancelled=true;}
  catch(const std::exception &e){result.errors.push_back(e.what());}
  return result;
}
}
std::vector<ArchiveFormat> WritableArchiveFormats() {
  std::vector<ArchiveFormat> result;
  for(const auto &f:formats)try {
    Writer w;w.Configure(f.id,6);
    FD fd(open("/dev/null",O_WRONLY));Require(fd.n>=0,"Cannot probe writer.");
    w.Check(LIBCALL(w.lib,archive_write_open_fd)(w.a,fd.n));
    w.Check(LIBCALL(w.lib,archive_write_close)(w.a));
    auto available=f;
    if(f.id=="zip") {
      auto probe=std::make_shared<ArchiveSecret>("Listi format probe");
      Writer encrypted;encrypted.Configure(f.id,6,probe);
      available.encryption=true;
    } else if(f.id=="7z" && SevenZipCanCreate()) {
      available.encryption=true;
      available.encryptedNames=true;
    }
    result.push_back(available);
  }catch(const std::exception &){}
  return result;
}
Job PrepareArchiveJob(bool extract,const std::vector<std::string> &sources) {
  Require(!sources.empty(),"Select files or folders first.");
  Job job;job.action=extract?Action::ExtractArchive:Action::CreateArchive;job.sources=sources;
  Require(!extract || sources.size()==1 || ParseLocation(sources[0]).Archive(),"Select one archive, or entries inside one archive.");
  std::set<std::string> names;
  for(const auto &source:sources) {
    const auto local=LocalSource(source);struct stat s;
    Require(!lstat(local.c_str(),&s),"Source is unavailable.");
    Require(S_ISREG(s.st_mode)||(!extract&&S_ISDIR(s.st_mode)),"Select regular files or folders, not links.");
    if(!extract)Require(!IsVirtual(source),"Copy archive entries to a local folder before packing them.");
    Require(names.insert(LocationLeaf(source)).second,"Selected entries have duplicate names.");
    job.expectedSources[local]={int32_t(s.st_dev),int64_t(s.st_ino)};
    if(extract){
      auto set=FindArchiveVolumes(local);job.expectedArchiveSets[set.files[0].path]=set.Fingerprint();
      for(const auto &part:set.files){struct stat st;Require(!lstat(part.path.c_str(),&st),"Archive part disappeared.");job.expectedSources[part.path]={int32_t(st.st_dev),int64_t(st.st_ino)};}
      Require(FindArchiveVolumes(local).Fingerprint()==set.Fingerprint(),"Archive parts changed during selection.");
    }
  }
  return job;
}
Job PrepareArchiveEditTarget(const std::string &archive,
    const std::string &internalFolder) {
  Require(!archive.empty()&&archive[0]=='/'&&!IsVirtual(archive),"Choose one local writable archive.");
  Job job;job.action=Action::EditArchive;job.destination=archive;job.archiveFormat=EditFormat(archive);
  struct stat archiveStat;Require(!lstat(archive.c_str(),&archiveStat)&&S_ISREG(archiveStat.st_mode),"Archive is unavailable.");
  job.expectedSources[archive]={int32_t(archiveStat.st_dev),int64_t(archiveStat.st_ino)};
  auto set=FindArchiveVolumes(archive);job.expectedArchiveSets[set.files[0].path]=set.Fingerprint();
  job.destinationFolder=ParentLocation(archive);job.entryName=Leaf(archive);
  struct stat parent;Require(!stat(job.destinationFolder.c_str(),&parent)&&S_ISDIR(parent.st_mode),"Archive folder is unavailable.");
  job.expectedDestination={int32_t(parent.st_dev),int64_t(parent.st_ino)};
  job.archivePath=MemberPath(internalFolder);
  return job;
}
void SetArchiveEditAdditions(Job &job,const std::vector<std::string> &add) {
  Require(job.action==Action::EditArchive&&!job.destination.empty(),"Archive edit target is missing.");
  Require(!add.empty(),"Choose files or folders to add.");
  VerifyArchiveSets(job);Guards(job);
  Job updated=job;for(const auto &source:updated.sources)updated.expectedSources.erase(source);
  updated.sources.clear();
  std::set<std::string> names;
  for(const auto &source:add){
    const auto location=ParseLocation(source);
    Require(location.Local()||location.Archive(), "Copy network files to a local folder before adding them to an archive.");
    Require(source!=job.destination,"An archive cannot be added to itself while it is being edited.");
    const auto physical=location.Archive()?location.archive:source;
    struct stat st;Require(!lstat(physical.c_str(),&st)&&(S_ISREG(st.st_mode)||(!location.Archive()&&S_ISDIR(st.st_mode))),
      "Only regular files, folders and archive entries can be added.");
    Require(!location.Archive()||!MemberPath(location.path).empty(), "Select entries inside an archive, not its root.");
    Require(names.insert(LocationLeaf(source)).second,"Selected additions have duplicate names.");
    updated.sources.push_back(source);updated.expectedSources[physical]={int32_t(st.st_dev),int64_t(st.st_ino)};
    if(location.Archive()) {
      auto set=FindArchiveVolumes(physical);
      updated.expectedArchiveSets[set.files.front().path]=set.Fingerprint();
    }
  }
  job=std::move(updated);
}
Job PrepareArchiveEditJob(const std::string &archive,
    const std::vector<std::string> &add,const std::vector<std::string> &remove,
    const std::string &internalFolder) {
  Require(!add.empty()||!remove.empty(),"Choose files to add or archive entries to remove.");
  Job job=PrepareArchiveEditTarget(archive,internalFolder);
  if(!add.empty())SetArchiveEditAdditions(job,add);
  std::set<std::string> removed;
  for(const auto &source:remove){auto location=ParseLocation(source);
    Require(location.Archive()&&location.archive==archive&&location.path!="/","Select entries from one archive, not the archive root.");
    removed.insert(MemberPath(location.path));}
  for(const auto &path:removed){bool covered=false;for(const auto &kept:job.archiveRemove)
      if(path.size()>kept.size()&&path.compare(0,kept.size(),kept)==0&&path[kept.size()]=='/'){covered=true;break;}
    if(!covered)job.archiveRemove.push_back(path);}
  return job;
}
void FreezeArchiveTargets(Job &transfer) {
  auto capture=[&](const std::string &path) {
    const auto location=ParseLocation(path);if(!location.Archive())return;
    const auto set=FindArchiveVolumes(location.archive);
    transfer.expectedArchiveSets[set.files.front().path]=set.Fingerprint();
  };
  capture(transfer.destination);for(const auto &source:transfer.sources)capture(source);
}
Job PrepareArchiveMutation(const Job &transfer) {
  VerifyArchiveSets(transfer);
  auto location=ParseLocation(transfer.destination);
  if (transfer.action==Action::Delete || transfer.action==Action::Trash) {
    Require(!transfer.sources.empty(), "Select archive entries first.");
    location=ParseLocation(transfer.sources.front());
    return PrepareArchiveEditJob(location.archive,{},transfer.sources);
  }
  Require(location.Archive(), "Choose a destination inside an archive.");
  if (transfer.action == Action::Move && !transfer.sources.empty() &&
      ParseLocation(transfer.sources.front()).archive != location.archive)
    return PrepareArchiveMove(transfer);
  auto edit=PrepareArchiveEditTarget(location.archive);
  if (transfer.action==Action::MakeDirectory) {
    edit.archiveFolders.push_back(MemberPath(location.path));
  } else {
    Require(transfer.action==Action::Rename||transfer.action==Action::Move,
      "This action cannot change archive entries.");
    Require(!transfer.sources.empty() && (transfer.action!=Action::Rename||transfer.sources.size()==1),
      "Choose one archive entry to rename.");
    for(const auto &path:transfer.sources) {
      const auto from=ParseLocation(path);
      Require(from.Archive()&&from.archive==location.archive,
        "Move between archives or local folders is not supported yet. Use Copy; originals are retained.");
      const auto to=transfer.action==Action::Rename ? location.path :
        ParseLocation(ChildLocation(transfer.destination,LocationLeaf(path))).path;
      const auto source=MemberPath(from.path),destination=MemberPath(to);
      Require(!source.empty()&&!destination.empty()&&source!=destination&&
        destination.compare(0,source.size()+1,source+"/")!=0,
        "An archive entry cannot be moved onto itself or into its contents.");
      edit.archiveRenames[source]=destination;
    }
  }
  return edit;
}
Job PrepareArchiveCopy(const Job &transfer) {
  VerifyArchiveSets(transfer);
  Require(transfer.action == Action::Copy,
    "Moving into archives is not supported. Use Copy; originals will be kept.");
  const auto target = ParseLocation(transfer.destination);
  Require(target.Archive(), "Choose a folder inside a local writable archive.");
  auto edit = PrepareArchiveEditJob(target.archive, transfer.sources, {}, target.path);
  for (const auto &item : transfer.expectedSources) {
    const auto found = edit.expectedSources.find(item.first);
    Require(found != edit.expectedSources.end() && found->second.device == item.second.device &&
      found->second.inode == item.second.inode, "A selected source was replaced. Select it again.");
  }
  return edit;
}
Job PrepareArchiveMove(const Job &transfer) {
  Require(!transfer.sources.empty() && transfer.sources.size() <= 1024,
    "Select between 1 and 1024 entries inside one writable archive.");
  Guards(transfer); VerifyArchiveSets(transfer);
  const auto target = ParseLocation(transfer.destination);
  const auto source = ParseLocation(transfer.sources.front());
  if (source.Local() != target.Local() && (source.Archive() || target.Archive())) {
    Require((source.Local() && target.Archive()) || (source.Archive() && target.Local()),
      "Move between a local folder and one writable local archive.");
    const auto archive = source.Archive() ? source.archive : target.archive;
    auto edit = PrepareArchiveEditTarget(archive, target.Archive() ? target.path : "");
    Job move = transfer; move.action = Action::MoveArchive;
    std::set<std::string> names;
    auto canonical = [](const std::string &path) {
      char *resolved = realpath(path.c_str(), nullptr);
      std::string result = resolved ? resolved : ""; free(resolved);
      Require(!result.empty(), "Cannot resolve local move path: " + path);
      return result;
    };
    const auto container = canonical(archive);
    const auto localTarget = target.Local() ? canonical(transfer.destination) : std::string();
    for (const auto &path : transfer.sources) {
      const auto location = ParseLocation(path);
      Require(location.Archive() == source.Archive() &&
        (source.Archive() ? location.archive == source.archive && !MemberPath(location.path).empty() : location.Local()),
        "Choose local entries or entries from one archive, not a mixed selection.");
      Require(!LocationLeaf(path).empty() && names.insert(LocationLeaf(path)).second,
        "Selected entries have duplicate or empty names.");
      auto local = source.Local() ? canonical(path) : ChildLocation(localTarget, LocationLeaf(path));
      Require(local != container && container.compare(0,local.size()+1,local+"/") != 0,
        "A move cannot replace or contain its own archive.");
    }
    for (const auto &path : transfer.sources)
      for (const auto &other : transfer.sources)
        Require(path == other || other.compare(0,path.size()+1,path+"/") != 0,
          "Select either a folder or its contents, not both.");
    FreezeLocalJob(move, target.Local() ? transfer.destination : edit.destinationFolder);
    move.expectedSources.insert(edit.expectedSources.begin(),edit.expectedSources.end());
    move.expectedArchiveSets = edit.expectedArchiveSets;
    Guards(transfer); VerifyArchiveSets(transfer);
    return move;
  }
  Require(source.Archive() && target.Archive(),
    "Archive Cut/Move requires a local writable archive at one or both ends.");
  auto from = PrepareArchiveEditTarget(source.archive);
  auto to = PrepareArchiveEditTarget(target.archive, target.path);
  const auto a = from.expectedSources.at(source.archive), b = to.expectedSources.at(target.archive);
  Require(source.archive == target.archive || a.device != b.device || a.inode != b.inode,
    "Source and destination are aliases of the same archive. Open the same archive path in both panels.");
  std::set<std::string> names, members;
  for (const auto &path : transfer.sources) {
    const auto location = ParseLocation(path);
    Require(location.Archive() && location.archive == source.archive && !MemberPath(location.path).empty(),
      "Move entries from one archive at a time, not its root.");
    Require(names.insert(LocationLeaf(path)).second, "Selected entries have duplicate names.");
    members.insert(MemberPath(location.path));
  }
  for (const auto &member : members)
    for (const auto &other : members)
      Require(member == other || other.compare(0, member.size()+1, member+"/") != 0,
        "Select either a folder or its contents, not both.");
  if (source.archive == target.archive)
    PrepareArchiveMutation(Job{Action::Move, transfer.sources, transfer.destination});
  Job move = transfer;
  move.action = Action::MoveArchive;
  move.expectedSources = from.expectedSources;
  move.expectedSources.insert(to.expectedSources.begin(), to.expectedSources.end());
  move.expectedArchiveSets = from.expectedArchiveSets;
  move.expectedArchiveSets.insert(to.expectedArchiveSets.begin(), to.expectedArchiveSets.end());
  move.destinationFolder = to.destinationFolder;
  move.expectedDestination = to.expectedDestination;
  // Do not refresh stale clipboard/confirmation guards during preparation.
  Guards(transfer); VerifyArchiveSets(transfer);
  return move;
}
namespace {
// Compare the durable destination's decoded bytes with the private source
// snapshot. A directory merge may retain additional destination entries.
void VerifyMovedEntry(const std::string &local, const std::string &destination,
                      const TransferControl &control, unsigned depth) {
  control.Check(); Require(depth <= 64, "Move verification exceeds the folder depth limit.");
  struct stat st; FileEntry target;
  Require(!lstat(local.c_str(), &st) && !S_ISLNK(st.st_mode) &&
    StatFile(destination, target, control) && !target.link &&
    bool(S_ISDIR(st.st_mode)) == target.directory, "Destination verification failed. Source entries were retained.");
  if (S_ISDIR(st.st_mode)) {
    for (const auto &child : Children(local))
      VerifyMovedEntry(child, ChildLocation(destination, Leaf(child)), control, depth+1);
    return;
  }
  Require(S_ISREG(st.st_mode) && target.sizeKnown && target.size == st.st_size,
    "Destination size verification failed. Source entries were retained.");
  FD input(open(local.c_str(), O_RDONLY|O_NOFOLLOW));
  Require(input.n >= 0, "Cannot read move verification snapshot.");
  std::vector<char> buffer(65536); int64_t total = 0;
  ReadFile(destination, [&](const char *bytes, size_t size) {
    while (size) {
      control.Check(); const auto amount = std::min(size, buffer.size());
      size_t have = 0;
      while (have < amount) {
        auto n = read(input.n, buffer.data()+have, amount-have);
        if (n < 0 && errno == EINTR) continue;
        Require(n > 0, "Move verification snapshot is incomplete."); have += n;
      }
      Require(!memcmp(bytes, buffer.data(), amount), "Destination bytes differ. Source entries were retained.");
      total += amount; bytes += amount; size -= amount;
    }
  }, control);
  Require(total == st.st_size, "Destination verification was incomplete. Source entries were retained.");
}
Result RunArchiveLocalMove(const Job &job, const Hooks &hooks) {
  Result result; bool copied = false, cleanup = false;
  try {
    const auto source = ParseLocation(job.sources.front());
    const auto target = ParseLocation(job.destination);
    Hooks internal = hooks; internal.fileDone = {}; internal.fileFailed = {};
    internal.proceed = [&] {
      if (hooks.proceed && !hooks.proceed()) throw Cancelled();
      Guards(job); VerifyArchiveSets(job); return true;
    };
    std::unique_ptr<LocalMoveTree> localSource;
    if (source.Local()) localSource.reset(new LocalMoveTree(job.sources, internal, true));
    Stage stage(job.destinationFolder);
    const auto snapshot = PathOf(stage.dir);
    Job extract{Action::Copy, job.sources, snapshot}; FreezeLocalJob(extract, snapshot);
    auto extracted = Run(extract, internal);
    if (extracted.cancelled) throw Cancelled();
    Require(extracted.errors.empty() && !extracted.skipped && extracted.completed == int(job.sources.size()),
      extracted.errors.empty() ? "Could not snapshot the complete selection. Sources retained." : extracted.errors.front());
    if (localSource) localSource->Verify();
    std::vector<std::string> additions, destinations;
    for (const auto &path : job.sources) {
      additions.push_back(Join(snapshot,LocationLeaf(path)));
      destinations.push_back(ChildLocation(job.destination,LocationLeaf(path)));
    }
    Job published;
    std::unique_ptr<LocalMoveTree> localDestination;
    if (target.Archive()) {
      auto add = PrepareArchiveEditJob(target.archive, additions, {}, target.path);
      auto written = RunArchiveEdit(add, internal);
      if (written.cancelled) throw Cancelled();
      Require(written.errors.empty() && written.completed == 1,
        written.errors.empty() ? "Destination archive update failed." : written.errors.front());
      copied = true;
      published = PrepareArchiveEditTarget(target.archive);
    } else {
      // Local directory merges may publish several files. A skipped or failed
      // file retains every source member; never treat a partial copy as a move.
      Job copy{Action::Copy, additions, job.destination}; FreezeLocalJob(copy,job.destination);
      copied = true;
      auto written = Run(copy, internal);
      if (written.cancelled) throw Cancelled();
      Require(written.errors.empty() && !written.skipped && written.completed == int(additions.size()),
        written.errors.empty() ? "Incomplete destination copy. Source archive retained." : written.errors.front());
      localDestination.reset(new LocalMoveTree(destinations,internal,false));
      localDestination->Flush();
    }
    auto destinationGuard = [&] {
      if (hooks.proceed && !hooks.proceed()) throw Cancelled();
      if (localDestination) localDestination->Verify();
      else { Guards(published); VerifyArchiveSets(published); }
      return true;
    };
    internal.proceed = destinationGuard;
    TransferControl verify; verify.proceed = destinationGuard;
    for (size_t i=0;i<additions.size();++i) VerifyMovedEntry(additions[i],destinations[i],verify);
    if (localSource) {
      localSource->Verify(); destinationGuard(); cleanup = true;
      for (const auto &path : job.sources) {
        localSource->Remove(path,internal);
        ++result.completed;
        const auto destination = ChildLocation(job.destination,LocationLeaf(path));
        result.completedRoots.emplace_back(path,destination);
        if (hooks.fileDone) hooks.fileDone(path,destination);
      }
    } else {
      Guards(job); VerifyArchiveSets(job);
      auto remove = PrepareArchiveEditJob(source.archive,{},job.sources);
      // Bind source and verified destination guards through the final rewrite.
      internal.proceed = [&] { destinationGuard(); Guards(job); VerifyArchiveSets(job); return true; };
      cleanup = true;
      const auto removed = RunArchiveEdit(remove,internal);
      if (removed.cancelled) throw Cancelled();
      Require(removed.errors.empty() && removed.completed == 1,
        removed.errors.empty() ? "Source cleanup did not complete." : removed.errors.front());
      for (const auto &path : job.sources) {
        ++result.completed;
        const auto destination = ChildLocation(job.destination,LocationLeaf(path));
        result.completedRoots.emplace_back(path,destination);
        if (hooks.fileDone) hooks.fileDone(path,destination);
      }
    }
  } catch (const Cancelled &) { result.cancelled = true; }
    catch (const std::exception &e) { result.errors.push_back(e.what()); }
  if (copied && (result.cancelled || !result.errors.empty()))
    result.errors.push_back(cleanup
      ? "Destination copies were verified, but source cleanup was interrupted. Check both locations; remaining sources were retained."
      : "Destination copies may remain. Source entries were retained. Check both locations before retrying.");
  return result;
}
Result RunArchiveMove(const Job &job, const Hooks &hooks) {
  Result result; bool copied = false;
  try {
    Guards(job); VerifyArchiveSets(job);
    Require(job.action == Action::MoveArchive, "Invalid archive move job.");
    // Revalidate the complete structure even for decoded/queued jobs.
    PrepareArchiveMove(job);
    const auto source = ParseLocation(job.sources.front());
    const auto target = ParseLocation(job.destination);
    if (source.Local() || target.Local()) return RunArchiveLocalMove(job, hooks);
    Hooks internal = hooks; internal.fileDone = {}; internal.fileFailed = {};
    auto sourceGuard = PrepareArchiveEditJob(source.archive, {}, job.sources);
    auto checkSource = [&] {
      if (hooks.proceed && !hooks.proceed()) throw Cancelled();
      Guards(sourceGuard); VerifyArchiveSets(sourceGuard);
    };
    internal.proceed = [&] {
      checkSource(); Guards(job); VerifyArchiveSets(job); return true;
    };
    if (source.archive == target.archive) {
      auto rename = PrepareArchiveMutation(Job{Action::Move, job.sources, job.destination});
      result = RunArchiveEdit(rename, internal);
    } else {
      Stage stage(job.destinationFolder);
      const auto snapshot = PathOf(stage.dir);
      Job extract{Action::Copy, job.sources, snapshot}; FreezeLocalJob(extract, snapshot);
      auto extracted = Run(extract, internal);
      if (extracted.cancelled) throw Cancelled();
      Require(extracted.errors.empty() && !extracted.skipped && extracted.completed == int(job.sources.size()),
        extracted.errors.empty() ? "Could not snapshot the complete selection. Archives are unchanged." : extracted.errors.front());
      std::vector<std::string> additions;
      for (const auto &path : job.sources) additions.push_back(Join(snapshot, LocationLeaf(path)));
      auto add = PrepareArchiveEditJob(target.archive, additions, {}, target.path);
      auto written = RunArchiveEdit(add, internal);
      if (written.cancelled) throw Cancelled();
      Require(written.errors.empty() && written.completed == 1,
        written.errors.empty() ? "Destination archive update failed." : written.errors.front());
      copied = true;
      // Destination is now committed and fsynced. Never roll it back to stale
      // bytes if source cleanup fails; retain both copies and report the failure.
      auto published = PrepareArchiveEditTarget(target.archive);
      sourceGuard.expectedSources.insert(published.expectedSources.begin(), published.expectedSources.end());
      sourceGuard.expectedArchiveSets.insert(published.expectedArchiveSets.begin(), published.expectedArchiveSets.end());
      internal.proceed = [&] { checkSource(); Guards(published); return true; };
      TransferControl verify; verify.proceed = internal.proceed;
      for (const auto &path : job.sources)
        VerifyMovedEntry(Join(snapshot, LocationLeaf(path)), ChildLocation(job.destination, LocationLeaf(path)), verify);
      checkSource();
      // Rebuild the source only while the verified destination identity and
      // contents remain frozen, including the final pre-rename guard.
      result = RunArchiveEdit(sourceGuard, internal);
    }
    if (!result.cancelled && result.errors.empty() && result.completed == 1) {
      result.completedRoots.clear(); result.completed = job.sources.size();
      for (const auto &path : job.sources) {
        const auto destination = ChildLocation(job.destination, LocationLeaf(path));
        result.completedRoots.emplace_back(path, destination);
        if (hooks.fileDone) hooks.fileDone(path, destination);
      }
    } else {
      result.completed = 0; result.completedRoots.clear();
      if (copied) result.errors.push_back("Destination copy was saved, but source cleanup did not complete. Check both archives before retrying.");
    }
  } catch (const Cancelled &) {
    result.cancelled = true;
    if (copied) result.errors.push_back("Move cancelled after copying. Source entries were retained; destination copies may remain.");
  } catch (const std::exception &e) {
    result.errors.push_back(e.what());
    if (copied) result.errors.push_back("Destination copy was saved. Source cleanup did not complete; check both archives before retrying.");
  }
  return result;
}
}
void SetArchiveDestination(Job &job,const std::string &folder,const std::string &name) {
  Require(!folder.empty() && folder[0]=='/' && !IsVirtual(folder),"Choose a local destination folder.");
  Require(!name.empty() && name.size()<B_FILE_NAME_LENGTH && name!="." && name!=".." && name.find_first_of("/\\\r\n")==std::string::npos,
    "Enter a single file or folder name.");
  VerifyArchiveSets(job);
  Job sourceGuards=job;sourceGuards.destinationFolder.clear();Guards(sourceGuards);
  struct stat s;Require(!stat(folder.c_str(),&s)&&S_ISDIR(s.st_mode),"Destination is not a folder.");
  job.destinationFolder=folder;job.expectedDestination={int32_t(s.st_dev),int64_t(s.st_ino)};
  auto destination=Join(folder,name);struct stat existing;
  Require(lstat(destination.c_str(),&existing)!=0 && errno==ENOENT,
    "That name already exists or is unavailable. Choose a new name.");
  job.entryName=name;job.destination=destination;
}
Result RunArchiveJob(const Job &job,const Hooks &hooks) {
  // Editor workers and the operation queue must not publish competing edits.
  // Waiting remains cancellable; external processes are covered by fingerprints.
  static std::recursive_mutex edits;
  std::unique_lock<std::recursive_mutex> ownership(edits, std::defer_lock);
  if (job.action == Action::EditArchive || job.action == Action::MoveArchive) {
    while (!ownership.try_lock()) {
      if (hooks.proceed && !hooks.proceed()) { Result result; result.cancelled = true; return result; }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
  if(job.action==Action::MoveArchive)return RunArchiveMove(job,hooks);
  if(job.action==Action::EditArchive)return RunArchiveEdit(job,hooks);
  Result result;
  std::set<std::string> completedPaths;
  Hooks tracked=hooks;
  tracked.progress=[&](const std::string &path,uint64_t bytes){
    completedPaths.insert(path);
    if(hooks.progress)hooks.progress(path,bytes);
  };
  try {
    Require(!job.sources.empty(),"Archive source selection is empty.");
    for(const auto &source:job.sources)Require(job.expectedSources.count(LocalSource(source)),"Archive source identity is missing.");
    Guards(job);VerifyArchiveSets(job);
    Require(!job.destinationFolder.empty()&&!job.entryName.empty(),"Archive destination is missing.");
    struct stat target;
    Require(lstat(job.destination.c_str(),&target)!=0 && errno==ENOENT,
      "The destination already exists or is unavailable. Choose another name; nothing was replaced.");
    for(const auto &source:job.sources) if(!IsVirtual(source)) {
      char *src=realpath(source.c_str(),nullptr),*dst=realpath(job.destinationFolder.c_str(),nullptr);
      std::string a=src?src:"",b=dst?dst:"";free(src);free(dst);
      Require(!a.empty()&&!b.empty(),"Cannot resolve archive paths.");
      Require(b!=a && b.compare(0,a.size()+1,a+"/")!=0,"Choose a destination outside the selected folder.");
    }
    Stage stage(job.destinationFolder);Guards(job);
    auto check=[&]{if(hooks.proceed&&!hooks.proceed())throw Cancelled();Guards(job);};
    check();BEntry publish;
    if(job.action==Action::CreateArchive) {
      auto path=Join(PathOf(stage.dir),"archive");
      FD output(open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600));Require(output.n>=0,"Cannot create archive output.");
      auto secret=TakeArchiveCreationPassword(job.archivePasswordToken);
      Require(!secret || job.archiveFormat=="zip" || job.archiveFormat=="7z",
        "The selected archive format does not support password protection.");
      Require(!job.archiveEncryptNames || (secret && job.archiveFormat=="7z"),
        "Only encrypted 7z archives can hide file names.");
      CreateOutput(output.n,job.archiveFormat,job.sources,job.archiveLevel,
        job.archiveEncryptNames,secret,tracked);
      Require(!fsync(output.n),"Cannot flush archive to disk.");
      Require(publish.SetTo(&stage.dir,"archive",false)==B_OK,"Archive output disappeared.");
    } else {
      std::vector<std::pair<std::string,struct stat>> original;
      for(const auto &item:job.expectedSources){struct stat s;Require(!lstat(item.first.c_str(),&s),"Archive disappeared.");original.push_back({item.first,s});}
      TransferControl control;control.proceed=[&]{check();return true;};
      auto selected=job.sources;
      if(!IsVirtual(selected[0])) {
        const auto root=OpenArchive(selected[0],control);Require(!root.empty(),"The selected file is not a supported archive.");
        selected.clear();for(const auto &entry:ListFiles(root,control))selected.push_back(entry.path);
      }
      Job copy;copy.sources=selected;copy.destination=PathOf(stage.dir);
      Hooks guarded=tracked;
      // Defer all member completion notifications until atomic publication.
      // Otherwise the UI leaves successfully extracted members marked failed.
      guarded.fileDone=[&](const std::string &path,const std::string &){completedPaths.insert(path);};
      guarded.fileFailed={};
      guarded.proceed=[&]{check();return true;};
      auto copied=Run(copy,guarded);
      if(copied.cancelled)throw Cancelled();
      Require(copied.errors.empty(),copied.errors.empty()?"":copied.errors.front());
      Require(!copied.skipped,"Archive contains conflicting entries; extraction was not published.");
      for(const auto &item:original){struct stat s;Require(!lstat(item.first.c_str(),&s)&&Unchanged(item.second,s),"Archive changed during extraction.");}
      Require(stage.dir.GetEntry(&publish)==B_OK,"Extraction staging folder disappeared.");
    }
    check();VerifyArchiveSets(job);
    // Native no-clobber publication. A new target appearing while the job runs
    // must never be overwritten, including an empty directory or a symlink.
    Require(publish.MoveTo(&stage.parent,job.entryName.c_str(),false)==B_OK,
      "Could not publish the result. The destination may already exist; it was not replaced.");
    if(job.action==Action::ExtractArchive)stage.dir.Unset();
    result.completed=1;
    completedPaths.insert(job.sources.begin(),job.sources.end());
    for(const auto &source:completedPaths)if(hooks.fileDone)hooks.fileDone(source,job.destination);
  }catch(const Cancelled &){result.cancelled=true;}
  catch(const std::exception &e){result.errors.push_back(e.what());}
  return result;
}
}
