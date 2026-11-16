// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace Listi {
// Streaming SHA-256 (FIPS 180-4). State is per comparison, with no global
// cache.
class Sha256 {
public:
  void Update(const void *, size_t);
  std::string Finish();

private:
  std::array<uint32_t, 8> state{{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                 0x510e527f, 0x9b05688c, 0x1f83d9ab,
                                 0x5be0cd19}};
  std::array<uint8_t, 64> buffer{};
  uint64_t bytes = 0;
  size_t used = 0;
  bool finished = false;
  void Block(const uint8_t *);
};
} // namespace Listi
