// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"

namespace Listi {
struct ImageOutput {
  uint32_t type;
  std::string mime;
  std::string name;
  std::string extension;
};

// Only guarded, single local regular-file drops may be converted.
bool CanProbeImageDrop(const Job &);
std::vector<ImageOutput> ImageOutputs(const Job &);
std::string ImageOutputName(const std::string &source,
                            const std::string &extension);
Result RunImageConversion(const Job &, const Hooks &);
}
