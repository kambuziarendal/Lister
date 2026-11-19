// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <stdexcept>

namespace Listi {
// Haiku can continue to acknowledge writes from its cache after device loss.
// A cross-volume copy must never authorize removal on the source filesystem.
inline void RequireSameMoveVolume(int32_t source, int32_t destination) {
  if (source < 0 || destination < 0 || source != destination)
    throw std::runtime_error(
        "Move between volumes is not supported. Use Copy instead; keep the "
        "originals until you have checked the copy on the destination volume.");
}

inline void CheckMoveVolumes(const Job &job) {
  for (const auto &entry : job.expectedSources)
    RequireSameMoveVolume(entry.second.device, job.expectedDestination.device);
}
} // namespace Listi
