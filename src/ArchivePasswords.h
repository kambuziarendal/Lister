// SPDX-License-Identifier: MIT
#pragma once
#include "VirtualFile.h"
#include <memory>
#include <vector>
#include <sys/stat.h>
namespace Listi {
// Non-copyable, explicitly cleared storage. Never serialized into jobs or URLs.
class ArchiveSecret {
  std::vector<char> bytes;

public:
  explicit ArchiveSecret(const char *text);
  ~ArchiveSecret();
  ArchiveSecret(const ArchiveSecret &) = delete;
  ArchiveSecret &operator=(const ArchiveSecret &) = delete;
  const char *Text() const { return bytes.data(); }
};
using ArchiveSecretPtr = std::shared_ptr<ArchiveSecret>;
using ArchivePasswordPrompt = std::function<ArchiveSecretPtr(
    const std::string &, bool, const TransferControl &)>;
void SetArchivePasswordPrompt(ArchivePasswordPrompt);
void ForgetArchivePasswords();
void StopArchivePasswords();
uint64_t ArchivePasswordGeneration();
void CheckArchivePasswordGeneration(uint64_t);
std::string ArchivePasswordIdentity(const struct stat &);
ArchiveSecretPtr GetArchivePassword(const std::string &path,
                                    const std::string &identity, bool retry,
                                    uint64_t generation,
                                    const TransferControl &);
void RememberArchivePassword(const std::string &identity, ArchiveSecretPtr,
                             uint64_t generation);
// Archive edits must preserve protection even if unrelated jobs evict the
// process-wide password cache. Observe only the selected input on this worker.
class ArchiveEditProtection {
public:
  ArchiveEditProtection(std::string fileIdentity, std::string setIdentity);
  ~ArchiveEditProtection();
  ArchiveEditProtection(const ArchiveEditProtection &) = delete;
  ArchiveEditProtection &operator=(const ArchiveEditProtection &) = delete;

  static bool Watching(const std::string &identity);
  static void Observe(const std::string &identity, const ArchiveSecretPtr &key = {});
  ArchiveSecretPtr Password() const;

private:
  static thread_local ArchiveEditProtection *active;
  ArchiveEditProtection *previous;
  const std::string fileIdentity, setIdentity;
  ArchiveSecretPtr password;
  bool protectedInput = false;
};
// Creation passwords live only in this process. The queue serializes the token,
// not the secret bytes. Taking a token removes it from the bounded registry.
uint64_t HoldArchiveCreationPassword(ArchiveSecretPtr);
ArchiveSecretPtr TakeArchiveCreationPassword(uint64_t);
void DiscardArchiveCreationPassword(uint64_t);
} // namespace Listi
