// SPDX-License-Identifier: MIT
#include "BulkRename.h"
#include "JobSnapshot.h"
#include "NamedEntry.h"
#include <map>
#include <set>
#include <stdexcept>
#include <sys/stat.h>

namespace Listi {
namespace {
bool SameNode(const struct stat &st, const NodeIdentity &identity) {
  return st.st_dev == static_cast<dev_t>(identity.device) &&
         st.st_ino == static_cast<ino_t>(identity.inode);
}

bool InspectSource(const Job &job, const std::string &source) {
  const auto before = Leaf(source);
  const auto original = job.expectedSources.find(source);
  struct stat st;
  if (before.empty() || before == "." || before == ".." ||
      Join(job.destinationFolder, before) != source ||
      original == job.expectedSources.end() || lstat(source.c_str(), &st) ||
      !SameNode(st, original->second))
    throw std::runtime_error("The selected entry changed or is unavailable.");
  if (S_ISDIR(st.st_mode) &&
      st.st_dev != static_cast<dev_t>(job.expectedDestination.device))
    throw std::runtime_error("Mounted volumes cannot be renamed.");
  return S_ISDIR(st.st_mode);
}

void InspectFolder(const Job &job) {
  struct stat st;
  if (job.action != Action::BulkRename || job.destinationFolder.empty() ||
      job.destinationFolder.front() != '/' ||
      stat(job.destinationFolder.c_str(), &st) || !S_ISDIR(st.st_mode) ||
      !SameNode(st, job.expectedDestination))
    throw std::runtime_error("The selected folder changed or is unavailable.");
}

std::string NewName(const std::string &leaf, bool folder,
                    const BulkRenameOptions &options, size_t index) {
  std::string stem = leaf, extension;
  const auto dot = leaf.find_last_of('.');
  if (!folder && dot != std::string::npos && dot > 0 && dot + 1 < leaf.size()) {
    stem = leaf.substr(0, dot);
    extension = leaf.substr(dot);
  }
  if (!options.search.empty()) {
    size_t pos = 0;
    while ((pos = stem.find(options.search, pos)) != std::string::npos) {
      stem.replace(pos, options.search.size(), options.replacement);
      pos += options.replacement.size();
    }
  }
  std::string number;
  if (options.number) {
    number = std::to_string(options.start + index);
    if (number.size() < options.digits)
      number.insert(0, options.digits - number.size(), '0');
    number.insert(0, " ");
  }
  return options.prefix + stem + options.suffix + number + extension;
}

// Validate the exact names that will be queued, and revalidate the same batch
// on the worker before the first write. An occupied name is always a blocker,
// even if another row in this batch would have freed it.
void CheckTargets(BulkRenamePreview &preview) {
  std::map<std::string, size_t> counts;
  for (const auto &row : preview.rows)
    if (row.error.empty())
      ++counts[row.after];
  for (auto &row : preview.rows) {
    if (row.error.empty() && row.before != row.after) {
      ++preview.changed;
      try {
        if (counts[row.after] > 1)
          throw std::runtime_error("More than one entry has this new name.");
        ValidateNewEntryName(preview.job.destinationFolder, row.after);
      } catch (const std::runtime_error &error) {
        row.error = error.what();
      }
    }
    if (!row.error.empty())
      ++preview.blocked;
  }
}
} // namespace

Job FreezeBulkRename(const std::string &folder,
                     const std::vector<std::string> &selection) {
  if (selection.size() < 2 || folder.empty() || folder.front() != '/')
    throw std::runtime_error(
        "Select at least two entries in one local folder.");
  Job job;
  job.action = Action::BulkRename;
  job.sources = selection;
  std::set<std::string> unique;
  for (const auto &source : selection) {
    if (Join(folder, Leaf(source)) != source ||
        !unique.insert(source).second)
      throw std::runtime_error(
          "Select distinct entries in the current local folder.");
  }
  FreezeLocalJob(job, folder);
  InspectFolder(job);
  for (const auto &source : selection)
    InspectSource(job, source);
  return job;
}

BulkRenamePreview PreviewBulkRename(const Job &job,
                                    const BulkRenameOptions &options) {
  BulkRenamePreview preview;
  preview.job = job;
  preview.job.bulkNames.clear();
  try {
    InspectFolder(job);
    if (options.number && (options.digits < 1 || options.digits > 9))
      throw std::runtime_error("Number width must be from 1 to 9.");
  } catch (const std::runtime_error &error) {
    preview.error = error.what();
    return preview;
  }
  for (size_t i = 0; i < job.sources.size(); ++i) {
    const auto &source = job.sources[i];
    BulkRenameRow row{Leaf(source), "", ""};
    try {
      const auto folder = InspectSource(job, source);
      row.after = NewName(row.before, folder, options, i);
      preview.job.bulkNames.emplace(source, row.after);
    } catch (const std::runtime_error &error) {
      row.error = error.what();
    }
    preview.rows.push_back(std::move(row));
  }
  CheckTargets(preview);
  return preview;
}

Result RunBulkRename(const Job &job, const Hooks &hooks) {
  Result result;
  for (const auto &source : job.sources) {
    const auto target = job.bulkNames.find(source);
    std::string destination;
    if (target != job.bulkNames.end())
      destination = Join(job.destinationFolder, target->second);
    result.outcomes.push_back({source, destination, "unprocessed", ""});
  }
  if (hooks.proceed && !hooks.proceed()) {
    result.cancelled = true;
    return result;
  }
  try {
    InspectFolder(job);
    if (job.sources.size() < 2 || job.bulkNames.size() != job.sources.size() ||
        job.expectedSources.size() != job.sources.size())
      throw std::runtime_error("The selected batch is incomplete.");
    BulkRenamePreview confirmed;
    confirmed.job = job;
    std::set<std::string> unique;
    for (const auto &source : job.sources) {
      const auto name = job.bulkNames.find(source);
      if (name == job.bulkNames.end() || !unique.insert(source).second)
        throw std::runtime_error("The selected batch is incomplete.");
      InspectSource(job, source);
      confirmed.rows.push_back({Leaf(source), name->second, ""});
    }
    CheckTargets(confirmed);
    if (!confirmed.CanApply()) {
      for (size_t i = 0; i < confirmed.rows.size(); ++i) {
        const auto &row = confirmed.rows[i];
        if (!row.error.empty()) {
          result.outcomes[i].status = "failed";
          result.outcomes[i].detail = row.error;
          result.errors.push_back(row.before + ": " + row.error);
        }
      }
      result.skipped = confirmed.changed - result.errors.size();
      if (result.errors.empty())
        result.errors.push_back("There are no changed names to apply.");
      return result;
    }
    size_t remaining = confirmed.changed;
    for (auto &outcome : result.outcomes) {
      const auto &source = outcome.source;
      const auto &name = job.bulkNames.at(source);
      if (Leaf(source) == name) {
        outcome.status = "skipped";
        outcome.detail = "Name unchanged.";
        continue;
      }
      Job rename;
      rename.action = Action::Rename;
      rename.sources.push_back(source);
      rename.destination = Join(job.destinationFolder, name);
      rename.destinationFolder = job.destinationFolder;
      rename.expectedSources.emplace(source, job.expectedSources.at(source));
      rename.expectedDestination = job.expectedDestination;
      auto row = Run(rename, hooks);
      if (!row.errors.empty()) {
        outcome.status = "failed";
        outcome.detail = row.errors.front();
      }
      if (row.completed) {
        outcome.status = "moved";
        ++result.completed;
        --remaining;
        result.completedRoots.emplace_back(source, rename.destination);
      }
      if (row.cancelled || !row.errors.empty()) {
        result.cancelled = row.cancelled;
        result.errors = std::move(row.errors);
        // Count unattempted changes separately from the row with an error.
        result.skipped = remaining - (result.errors.empty() ? 0 : 1);
        break;
      }
    }
  } catch (const std::runtime_error &error) {
    result.errors.push_back(error.what());
  }
  return result;
}
} // namespace Listi
