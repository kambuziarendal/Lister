// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "ArchiveBackend.h"
#include "ArchiveVolumes.h"
#include "DynamicLibrary.h"
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <Directory.h>
#include <Entry.h>
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
#include <memory>
#include <set>

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
  void Configure(const std::string &format, int level) {
    Require(level>=0 && level<=9, "Invalid compression level.");
    if(format=="zip") Check(LIBCALL(lib, archive_write_set_format_zip)(a));
    else if(format=="7z") Check(LIBCALL(lib, archive_write_set_format_7zip)(a));
    else {
      Require(format=="tar" || format=="tgz" || format=="txz", "Unknown archive format.");
      Check(LIBCALL(lib, archive_write_set_format_pax_restricted)(a));
      if(format=="tgz") Check(LIBCALL(lib, archive_write_add_filter_gzip)(a));
      if(format=="txz") Check(LIBCALL(lib, archive_write_add_filter_xz)(a));
    }
    if(format!="tar") {
      const std::string option="compression-level="+std::to_string(level);
      Check(LIBCALL(lib, archive_write_set_options)(a,option.c_str()));
    }
  }
};
const ArchiveFormat formats[]={{"zip","ZIP",".zip",true},{"7z","7z",".7z",true},
 {"tgz","Tar + gzip",".tar.gz",true},{"txz","Tar + xz",".tar.xz",true},{"tar","Tar (uncompressed)",".tar",false}};
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
    std::string pattern=Join(folder,".Listi-archive-XXXXXX");
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
}
std::vector<ArchiveFormat> WritableArchiveFormats() {
  std::vector<ArchiveFormat> result;
  for(const auto &f:formats)try {
    Writer w;w.Configure(f.id,6);
    FD fd(open("/dev/null",O_WRONLY));Require(fd.n>=0,"Cannot probe writer.");
    w.Check(LIBCALL(w.lib,archive_write_open_fd)(w.a,fd.n));
    w.Check(LIBCALL(w.lib,archive_write_close)(w.a));result.push_back(f);
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
      Writer writer;writer.Configure(job.archiveFormat,job.archiveLevel);
      writer.Check(LIBCALL(writer.lib,archive_write_open_fd)(writer.a,output.n));
      // Pack verifies every opened node and the complete captured tree. Avoid
      // re-statting all selected roots on every buffer (quadratic for many files).
      Pack pack{writer,tracked};
      for(const auto &source:job.sources)pack.Add(source,Leaf(source));
      writer.Check(LIBCALL(writer.lib,archive_write_close)(writer.a));
      pack.Verify();
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
