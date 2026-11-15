// SPDX-License-Identifier: MIT
#include "DirectoryScanner.h"
#include "Common.h"
#include "FileOperation.h"
#include "VirtualFile.h"
#include "Trash.h"
#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <MimeType.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
namespace {
class Scanner : public BLooper {
  BMessenger target;
  std::shared_ptr<std::atomic<int64>> generation;

public:
  Scanner(BMessenger t, std::shared_ptr<std::atomic<int64>> g)
      : BLooper("directory reader"), target(t), generation(g) {}
  void MessageReceived(BMessage *m) override {
    if (m->what != kScan) {
      BLooper::MessageReceived(m);
      return;
    }
    int64 serial = m->GetInt64("serial", 0);
    if (generation->load() != serial) return;
    BString path = m->GetString("path", "");
    bool icons = m->GetBool("icons", true);
    int32 iconPixels = m->GetInt32("iconSize", 16);
    if (m->GetBool("trash", false)) {
      try {
        path = Lister::TrashDirectory(path.String(), true).c_str();
      } catch (const std::exception &error) {
        BMessage failed(kScanDone);
        failed.AddInt64("serial", serial);
        failed.AddString("path", path);
        failed.AddString("error", error.what());
        target.SendMessage(&failed);
        return;
      }
    }
    BMessage batch(kRows);
    batch.AddInt64("serial", serial);
    int count = 0;
    auto send = [&]() {
      target.SendMessage(&batch, (BHandler *)nullptr, 500000);
      batch.MakeEmpty();
      batch.what = kRows;
      batch.AddInt64("serial", serial);
      count = 0;
    };
    auto row = [&](const char *full, const char *label, bool parent = false) {
      struct stat st;
      if (lstat(full, &st))
        return;
      BMessage item;
      item.AddInt32("iconSize", iconPixels);
      item.AddString("name", label);
      item.AddString("path", full);
      item.AddBool("parent", parent);
      bool link = S_ISLNK(st.st_mode);
      bool dir = S_ISDIR(st.st_mode);
      struct stat resolved;
      if (link && stat(full, &resolved) == 0)
        dir = S_ISDIR(resolved.st_mode);
      item.AddInt32("kind", link ? 2 : dir ? 1 : 0);
      item.AddBool("directory", dir);
      item.AddInt64("size", st.st_size);
      item.AddInt64("date", st.st_mtime);
      entry_ref ref;
      BEntry e(full, false);
      if (e.GetRef(&ref) == B_OK) {
        item.AddRef("ref", &ref);
        if (icons) {
          BBitmap bitmap(BRect(0, 0, iconPixels - 1, iconPixels - 1), B_RGBA32);
          if (BNodeInfo::GetTrackerIcon(
                  &ref, &bitmap, static_cast<icon_size>(iconPixels)) == B_OK)
            item.AddData("icon", B_RAW_TYPE, bitmap.Bits(),
                         bitmap.BitsLength());
        }
      }
      batch.AddMessage("entry", &item);
      if (++count == 80)
        send();
    };
    BMessage done(kScanDone);
    done.AddInt64("serial", serial);
    if (Lister::IsVirtual(path.String())) {
      Lister::TransferControl control;
      control.proceed = [&] { return generation->load() == serial; };
      auto virtualRow = [&](const Lister::FileEntry &entry,
                            bool parent = false) {
        BMessage item;
        item.AddInt32("iconSize", iconPixels);
        item.AddString("name", parent ? ".." : entry.name.c_str());
        item.AddString("path", entry.path.c_str());
        item.AddBool("parent", parent);
        item.AddBool("directory", entry.directory);
        item.AddInt32("kind", entry.link ? 2 : entry.directory ? 1 : 0);
        item.AddInt64("size", entry.size);
        item.AddBool("sizeKnown", entry.sizeKnown);
        item.AddInt64("date", entry.modified);
        if (icons) {
          BMimeType mime(entry.directory ? "application/x-vnd.Be-directory"
                                         : "application/octet-stream");
          if (!entry.directory) {
            BMimeType guessed;
            if (BMimeType::GuessMimeType(entry.name.c_str(), &guessed) == B_OK)
              mime.SetTo(guessed.Type());
          }
          BBitmap bitmap(BRect(0, 0, iconPixels - 1, iconPixels - 1), B_RGBA32);
          if (mime.GetIcon(&bitmap, static_cast<icon_size>(iconPixels)) == B_OK)
            item.AddData("icon", B_RAW_TYPE, bitmap.Bits(),
                         bitmap.BitsLength());
        }
        batch.AddMessage("entry", &item);
        if (++count == 80)
          send();
      };
      try {
        auto entries = Lister::ListFiles(path.String(), control);
        auto parent = Lister::ParentLocation(path.String());
        if (parent != path.String()) {
          Lister::FileEntry entry;
          entry.path = parent;
          entry.directory = true;
          virtualRow(entry, true);
        }
        for (auto &entry : entries) {
          control.Check();
          virtualRow(entry);
        }
      } catch (const Lister::Cancelled &) {
        return;
      } catch (const std::exception &error) {
        done.AddString("error", error.what());
      }
    } else if (path == "@disks") {
      BVolumeRoster roster;
      BVolume volume;
      while (generation->load() == serial &&
             roster.GetNextVolume(&volume) == B_OK) {
        BDirectory root;
        BEntry e;
        BPath p;
        char name[B_FILE_NAME_LENGTH];
        if (volume.GetRootDirectory(&root) == B_OK &&
            root.GetEntry(&e) == B_OK && e.GetPath(&p) == B_OK &&
            volume.GetName(name) == B_OK)
          row(p.Path(), name);
      }
    } else {
      char resolved[PATH_MAX];
      if (!realpath(path.String(), resolved)) {
        done.AddString("error", strerror(errno));
      } else {
        path = resolved;
        DIR *dir = opendir(path.String());
        if (!dir)
          done.AddString("error", strerror(errno));
        else {
          if (path != "/") {
            std::string parent = path.String();
            parent = parent.substr(0, parent.find_last_of('/'));
            if (parent.empty())
              parent = "/";
            row(parent.c_str(), "..", true);
          }
          while (generation->load() == serial) {
            errno = 0;
            auto e = readdir(dir);
            if (!e) {
              if (errno)
                done.AddString("error", strerror(errno));
              break;
            }
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") ||
                Lister::IsListiPrivateEntry(e->d_name))
              continue;
            row(Lister::Join(path.String(), e->d_name).c_str(), e->d_name);
          }
          closedir(dir);
          BNode node(path.String());
          node_ref ref;
          if (node.GetNodeRef(&ref) == B_OK)
            done.AddData("node", B_RAW_TYPE, &ref, sizeof(ref));
        }
      }
    }
    if (generation->load() == serial) {
      if (count)
        send();
      done.AddString("path", path);
      target.SendMessage(&done, (BHandler *)nullptr, 500000);
    }
  }
};
}
BLooper *CreateDirectoryScanner(BMessenger target,
    std::shared_ptr<std::atomic<int64>> generation) {
  return new Scanner(target, generation);
}
