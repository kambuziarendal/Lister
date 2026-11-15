// SPDX-License-Identifier: MIT
#include "JobSnapshot.h"
#include "VirtualFile.h"
#include <sys/stat.h>
namespace Lister {
void FreezeLocalJob(Job &job, const std::string &folder) {
  if (IsVirtual(folder)) return;
  for (const auto &path : job.sources) if (IsVirtual(path)) return;
  for (const auto &path : job.sources) {
    struct stat st;
    if (lstat(path.c_str(), &st)) throw std::runtime_error("Selected entry is unavailable. Select it again.");
    job.expectedSources[path] = {int32_t(st.st_dev), int64_t(st.st_ino)};
  }
  if (!folder.empty()) {
    struct stat st;
    if (stat(folder.c_str(), &st) || !S_ISDIR(st.st_mode))
      throw std::runtime_error("The destination folder is unavailable.");
    job.expectedDestination = {int32_t(st.st_dev), int64_t(st.st_ino)};
    job.destinationFolder = folder;
  }
}
BMessage EncodeJob(const Job &job, uint32 what) {
  BMessage m(what);
  m.AddInt32("action", int(job.action));
  m.AddString("archiveFormat", job.archiveFormat.c_str());
  m.AddInt32("archiveLevel", job.archiveLevel);
  m.AddInt64("archivePasswordToken", job.archivePasswordToken);
  m.AddBool("archiveEncryptNames", job.archiveEncryptNames);
  m.AddString("entryName", job.entryName.c_str());
  m.AddString("destination", job.destination.c_str());
  m.AddString("destinationFolder", job.destinationFolder.c_str());
  m.AddInt32("destinationDevice", job.expectedDestination.device);
  m.AddInt64("destinationNode", job.expectedDestination.inode);
  for (const auto &path : job.sources) m.AddString("source", path.c_str());
  for (const auto &entry : job.expectedSources) {
    BMessage item;
    item.AddString("path", entry.first.c_str());
    item.AddInt32("device", entry.second.device);
    item.AddInt64("node", entry.second.inode);
    m.AddMessage("identity", &item);
  }
  for (const auto &entry : job.expectedArchiveSets) {
    BMessage item;item.AddString("path",entry.first.c_str());item.AddString("fingerprint",entry.second.c_str());m.AddMessage("archiveSet",&item);
  }
  return m;
}
Job DecodeJob(const BMessage &m) {
  Job job;
  job.action = static_cast<Action>(m.GetInt32("action", 0));
  job.archiveFormat = m.GetString("archiveFormat", "zip");
  job.archiveLevel = m.GetInt32("archiveLevel", 6);
  job.archivePasswordToken = m.GetInt64("archivePasswordToken", 0);
  job.archiveEncryptNames = m.GetBool("archiveEncryptNames", false);
  job.entryName = m.GetString("entryName", "");
  job.destination = m.GetString("destination", "");
  job.destinationFolder = m.GetString("destinationFolder", "");
  job.expectedDestination = {m.GetInt32("destinationDevice", -1), m.GetInt64("destinationNode", -1)};
  const char *path;
  for (int32 i = 0; m.FindString("source", i, &path) == B_OK; ++i) job.sources.emplace_back(path);
  BMessage item;
  for (int32 i = 0; m.FindMessage("identity", i, &item) == B_OK; ++i)
    job.expectedSources[item.GetString("path", "")] = {item.GetInt32("device", -1), item.GetInt64("node", -1)};
  for (int32 i=0;m.FindMessage("archiveSet",i,&item)==B_OK;++i)
    job.expectedArchiveSets[item.GetString("path","")]=item.GetString("fingerprint","");
  return job;
}
}
