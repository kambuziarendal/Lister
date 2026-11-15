// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <string>
#include <vector>
namespace Lister {
struct ArchiveFormat { std::string id, label, extension; bool compression; };
std::vector<ArchiveFormat> WritableArchiveFormats();
Job PrepareArchiveJob(bool extract, const std::vector<std::string> &sources);
void SetArchiveDestination(Job &, const std::string &folder, const std::string &name);
Result RunArchiveJob(const Job &, const Hooks &);
}
