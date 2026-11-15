// SPDX-License-Identifier: MIT
#include "ArchivePasswords.h"
#include <chrono>
#include <cstring>
#include <list>
#include <mutex>
namespace Lister {
ArchiveSecret::ArchiveSecret(const char *text) {
  size_t length = strlen(text);
  if (!length || length > 1024) throw std::runtime_error("Enter a password of 1 to 1024 bytes.");
  bytes.assign(text, text + length + 1);
}
ArchiveSecret::~ArchiveSecret() {
  volatile char *p = bytes.data();
  for (size_t i = 0; i < bytes.size(); ++i) p[i] = 0;
}
namespace {
std::mutex mutex;
std::timed_mutex promptMutex;
ArchivePasswordPrompt prompt;
uint64_t generation = 0;
bool stopping = false;
std::list<std::pair<std::string, ArchiveSecretPtr>> cache;
}
void SetArchivePasswordPrompt(ArchivePasswordPrompt fn) {
  std::lock_guard<std::mutex> lock(mutex);prompt = std::move(fn);stopping = false;
}
void ForgetArchivePasswords() {
  std::lock_guard<std::mutex> lock(mutex);cache.clear();++generation;
}
void StopArchivePasswords() {
  std::lock_guard<std::mutex> lock(mutex);stopping = true;cache.clear();++generation;prompt = {};
}
uint64_t ArchivePasswordGeneration() {
  std::lock_guard<std::mutex> lock(mutex);return generation;
}
void CheckArchivePasswordGeneration(uint64_t expected) {
  std::lock_guard<std::mutex> lock(mutex);
  if (stopping || generation != expected) throw Cancelled();
}
std::string ArchivePasswordIdentity(const struct stat &s) {
  return std::to_string(s.st_dev)+":"+std::to_string(s.st_ino)+":"+
    std::to_string(s.st_size)+":"+std::to_string(s.st_mtim.tv_sec)+":"+
    std::to_string(s.st_mtim.tv_nsec)+":"+std::to_string(s.st_ctim.tv_sec)+":"+
    std::to_string(s.st_ctim.tv_nsec);
}
ArchiveSecretPtr GetArchivePassword(const std::string &path,
    const std::string &identity, bool retry, uint64_t expected,
    const TransferControl &control) {
  auto check = [&] { control.Check();CheckArchivePasswordGeneration(expected); };
  check();
  std::unique_lock<std::timed_mutex> gate(promptMutex, std::defer_lock);
  while (!gate.try_lock_for(std::chrono::milliseconds(100))) check();
  check();
  ArchivePasswordPrompt ask;
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = cache.begin(); it != cache.end(); ++it) {
      if (it->first != identity) continue;
      if (!retry) {auto result = it->second;cache.splice(cache.begin(),cache,it);return result;}
      cache.erase(it);break;
    }
    ask = prompt;
  }
  if (!ask) throw std::runtime_error("This archive needs a password. Open it in Listi to enter one.");
  TransferControl waiting;waiting.proceed = [&] {check();return true;};
  auto result = ask(path,retry,waiting);check();
  if (!result) throw Cancelled();
  return result;
}
void RememberArchivePassword(const std::string &identity, ArchiveSecretPtr secret,
    uint64_t expected) {
  if (!secret) return;
  std::lock_guard<std::mutex> lock(mutex);
  if (stopping || generation != expected) return;
  cache.remove_if([&](const auto &item){return item.first == identity;});
  cache.emplace_front(identity,std::move(secret));
  while(cache.size()>16)cache.pop_back();
}
} // namespace Lister
