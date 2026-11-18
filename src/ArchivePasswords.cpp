// SPDX-License-Identifier: MIT
#include "ArchivePasswords.h"
#include <chrono>
#include <cstring>
#include <list>
#include <map>
#include <mutex>
namespace Listi {
ArchiveSecret::ArchiveSecret(const char *text) {
  size_t length = strlen(text);
  if (!length || length > 1024)
    throw std::runtime_error("Enter a password of 1 to 1024 bytes.");
  bytes.assign(text, text + length + 1);
}
ArchiveSecret::~ArchiveSecret() {
  volatile char *p = bytes.data();
  for (size_t i = 0; i < bytes.size(); ++i)
    p[i] = 0;
}
namespace {
std::mutex mutex;
std::timed_mutex promptMutex;
ArchivePasswordPrompt prompt;
uint64_t generation = 0;
bool stopping = false;
std::list<std::pair<std::string, ArchiveSecretPtr>> cache;
std::map<uint64_t, ArchiveSecretPtr> creation;
uint64_t nextCreationToken = 1;
}
void SetArchivePasswordPrompt(ArchivePasswordPrompt fn) {
  std::lock_guard<std::mutex> lock(mutex);
  prompt = std::move(fn);
  stopping = false;
}
void ForgetArchivePasswords() {
  std::lock_guard<std::mutex> lock(mutex);
  cache.clear();
  creation.clear();
  ++generation;
}
void StopArchivePasswords() {
  std::lock_guard<std::mutex> lock(mutex);
  stopping = true;
  cache.clear();
  creation.clear();
  ++generation;
  prompt = {};
}
uint64_t ArchivePasswordGeneration() {
  std::lock_guard<std::mutex> lock(mutex);
  return generation;
}
void CheckArchivePasswordGeneration(uint64_t expected) {
  std::lock_guard<std::mutex> lock(mutex);
  if (stopping || generation != expected)
    throw Cancelled();
}
std::string ArchivePasswordIdentity(const struct stat &s) {
  return std::to_string(s.st_dev) + ":" + std::to_string(s.st_ino) + ":" +
         std::to_string(s.st_size) + ":" + std::to_string(s.st_mtim.tv_sec) +
         ":" + std::to_string(s.st_mtim.tv_nsec) + ":" +
         std::to_string(s.st_ctim.tv_sec) + ":" +
         std::to_string(s.st_ctim.tv_nsec);
}
ArchiveSecretPtr GetArchivePassword(const std::string &path,
                                    const std::string &identity, bool retry,
                                    uint64_t expected,
                                    const TransferControl &control) {
  auto check = [&] {
    control.Check();
    CheckArchivePasswordGeneration(expected);
  };
  check();
  std::unique_lock<std::timed_mutex> gate(promptMutex, std::defer_lock);
  while (!gate.try_lock_for(std::chrono::milliseconds(100)))
    check();
  check();
  ArchivePasswordPrompt ask;
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = cache.begin(); it != cache.end(); ++it) {
      if (it->first != identity)
        continue;
      if (!retry) {
        auto result = it->second;
        cache.splice(cache.begin(), cache, it);
        return result;
      }
      cache.erase(it);
      break;
    }
    ask = prompt;
  }
  if (!ask)
    throw std::runtime_error(
        "This archive needs a password. Open it in Listi to enter one.");
  TransferControl waiting;
  waiting.proceed = [&] {
    check();
    return true;
  };
  auto result = ask(path, retry, waiting);
  check();
  if (!result)
    throw Cancelled();
  return result;
}
void RememberArchivePassword(const std::string &identity,
                             ArchiveSecretPtr secret, uint64_t expected) {
  if (!secret)
    return;
  std::lock_guard<std::mutex> lock(mutex);
  if (stopping || generation != expected)
    return;
  ArchiveEditProtection::Observe(identity, secret);
  cache.remove_if([&](const auto &item) { return item.first == identity; });
  cache.emplace_front(identity, std::move(secret));
  while (cache.size() > 16)
    cache.pop_back();
}
thread_local ArchiveEditProtection *ArchiveEditProtection::active = nullptr;

ArchiveEditProtection::ArchiveEditProtection(std::string file, std::string set)
    : previous(active), fileIdentity(std::move(file)), setIdentity(std::move(set)) {
  active = this;
}
ArchiveEditProtection::~ArchiveEditProtection() { active = previous; }

bool ArchiveEditProtection::Watching(const std::string &identity) {
  return active && (identity == active->fileIdentity || identity == active->setIdentity);
}
void ArchiveEditProtection::Observe(const std::string &identity, const ArchiveSecretPtr &key) {
  if (!Watching(identity))
    return;
  active->protectedInput = true;
  if (!key)
    return;
  if (active->password && strcmp(active->password->Text(), key->Text()))
    throw std::runtime_error("Cannot preserve different member passwords when editing this archive.");
  active->password = key;
}
ArchiveSecretPtr ArchiveEditProtection::Password() const {
  if (protectedInput && !password)
    throw std::runtime_error("Cannot verify the archive password for this edit. Original archive retained.");
  return password;
}
uint64_t HoldArchiveCreationPassword(ArchiveSecretPtr secret) {
  if (!secret)
    return 0;
  std::lock_guard<std::mutex> lock(mutex);
  if (stopping)
    throw Cancelled();
  if (creation.size() >= 16)
    throw std::runtime_error(
        "Finish or cancel a pending encrypted archive job before queuing another one.");
  while (!nextCreationToken || creation.count(nextCreationToken))
    ++nextCreationToken;
  const auto token = nextCreationToken++;
  creation.emplace(token, std::move(secret));
  return token;
}
ArchiveSecretPtr TakeArchiveCreationPassword(uint64_t token) {
  if (!token)
    return {};
  std::lock_guard<std::mutex> lock(mutex);
  auto found = creation.find(token);
  if (stopping || found == creation.end())
    throw Cancelled();
  auto result = std::move(found->second);
  creation.erase(found);
  return result;
}
void DiscardArchiveCreationPassword(uint64_t token) {
  std::lock_guard<std::mutex> lock(mutex);
  creation.erase(token);
}
} // namespace Listi
