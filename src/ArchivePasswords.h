// SPDX-License-Identifier: MIT
#pragma once
#include "VirtualFile.h"
#include <memory>
#include <vector>
#include <sys/stat.h>
namespace Lister {
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
    const std::string &identity, bool retry, uint64_t generation,
    const TransferControl &);
void RememberArchivePassword(const std::string &identity, ArchiveSecretPtr,
    uint64_t generation);
// Returns a matching in-memory password without prompting. The complete file
// identity must still match, so a replaced archive never inherits a secret.
ArchiveSecretPtr CachedArchivePassword(const std::string &path,
    const std::string &alternateIdentity = {});
// Creation passwords live only in this process. The queue serializes the token,
// not the secret bytes. Taking a token removes it from the bounded registry.
uint64_t HoldArchiveCreationPassword(ArchiveSecretPtr);
ArchiveSecretPtr TakeArchiveCreationPassword(uint64_t);
void DiscardArchiveCreationPassword(uint64_t);
} // namespace Lister
