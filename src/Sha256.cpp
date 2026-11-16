// SPDX-License-Identifier: MIT
#include "Sha256.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace Listi {
namespace {
constexpr uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
uint32_t R(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
} // namespace
void Sha256::Block(const uint8_t *p) {
  uint32_t w[64];
  for (unsigned i = 0; i < 16; ++i)
    w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
           (uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
  for (unsigned i = 16; i < 64; ++i) {
    auto a = w[i - 15], b = w[i - 2];
    w[i] = w[i - 16] + (R(a, 7) ^ R(a, 18) ^ (a >> 3)) + w[i - 7] +
           (R(b, 17) ^ R(b, 19) ^ (b >> 10));
  }
  auto a = state[0], b = state[1], c = state[2], d = state[3], e = state[4],
       f = state[5], g = state[6], h = state[7];
  for (unsigned i = 0; i < 64; ++i) {
    auto t1 = h + (R(e, 6) ^ R(e, 11) ^ R(e, 25)) + ((e & f) ^ (~e & g)) +
              k[i] + w[i];
    auto t2 = (R(a, 2) ^ R(a, 13) ^ R(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}
void Sha256::Update(const void *data, size_t size) {
  if (finished)
    throw std::logic_error("SHA-256 already finished");
  if (size > (UINT64_MAX / 8) - bytes)
    throw std::overflow_error("SHA-256 input too large");
  bytes += size;
  auto p = static_cast<const uint8_t *>(data);
  while (size) {
    auto n = std::min(size, buffer.size() - used);
    memcpy(buffer.data() + used, p, n);
    used += n;
    p += n;
    size -= n;
    if (used == 64) {
      Block(buffer.data());
      used = 0;
    }
  }
}
std::string Sha256::Finish() {
  if (finished)
    throw std::logic_error("SHA-256 already finished");
  finished = true;
  uint64_t bits = bytes * 8;
  buffer[used++] = 0x80;
  if (used > 56) {
    std::fill(buffer.begin() + used, buffer.end(), 0);
    Block(buffer.data());
    used = 0;
  }
  std::fill(buffer.begin() + used, buffer.begin() + 56, 0);
  for (unsigned i = 0; i < 8; ++i)
    buffer[63 - i] = uint8_t(bits >> (i * 8));
  Block(buffer.data());
  const char *hex = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (auto word : state)
    for (int i = 28; i >= 0; i -= 4)
      result += hex[(word >> i) & 15];
  return result;
}
} // namespace Listi
