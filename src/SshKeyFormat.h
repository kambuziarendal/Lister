// SPDX-License-Identifier: MIT
#pragma once

#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace Listi {
namespace SshKeyHeader {
class Input {
  int file;
  size_t remaining = 1024;
  uint32_t decoded = 0;
  unsigned available = 0;

  static int Base64(int c) {
    if (c >= 'A' && c <= 'Z')
      return c - 'A';
    if (c >= 'a' && c <= 'z')
      return c - 'a' + 26;
    if (c >= '0' && c <= '9')
      return c - '0' + 52;
    if (c == '+' || c == '/')
      return c == '+' ? 62 : 63;
    return -1;
  }

  int Get() {
    if (file < 0 || !remaining)
      return EOF;
    --remaining;
    unsigned char byte;
    return read(file, &byte, 1) == 1 ? byte : EOF;
  }

public:
  explicit Input(const std::string &path)
      : file(open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)) {
    // Do not block on pipes or consult devices while probing a file field.
    struct stat info;
    if (file >= 0 && (fstat(file, &info) != 0 || !S_ISREG(info.st_mode))) {
      close(file);
      file = -1;
    }
  }

  ~Input() {
    if (file >= 0)
      close(file);
  }
  Input(const Input &) = delete;
  Input &operator=(const Input &) = delete;

  bool Line(std::string &line) {
    line.clear();
    for (int c; (c = Get()) != EOF && line.size() < 128;) {
      if (c == '\n') {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        return true;
      }
      line += static_cast<char>(c);
    }
    return false;
  }

  int Byte() {
    while (available < 8) {
      int c = Get();
      while (c == '\n' || c == '\r')
        c = Get();
      const int value = Base64(c);
      if (value < 0)
        return -1;
      decoded = (decoded << 6) | value;
      available += 6;
    }
    available -= 8;
    return (decoded >> available) & 0xff;
  }

  bool Skip(size_t count) {
    while (count--)
      if (Byte() < 0)
        return false;
    return true;
  }

  bool UInt(uint32_t &value) {
    value = 0;
    for (int i = 0; i < 4; ++i) {
      const int byte = Byte();
      if (byte < 0)
        return false;
      value = (value << 8) | static_cast<unsigned>(byte);
    }
    return true;
  }

  bool SshName(std::string &name) {
    uint32_t length;
    if (!UInt(length) || length == 0 || length > 64)
      return false;
    name.clear();
    for (uint32_t i = 0; i < length; ++i) {
      const int byte = Byte();
      if (byte < 33 || byte > 126 || byte == ',')
        return false;
      name += static_cast<char>(byte);
    }
    return true;
  }

  bool DerSize(uint32_t &length) {
    int byte = Byte();
    if (byte < 0)
      return false;
    if (byte < 128) {
      length = byte;
      return true;
    }
    const int count = byte & 0x7f;
    if (count < 1 || count > 4)
      return false;
    length = 0;
    for (int i = 0; i < count; ++i) {
      byte = Byte();
      if (byte < 0 || (i == 0 && byte == 0))
        return false;
      length = (length << 8) | static_cast<unsigned>(byte);
    }
    return length >= 128;
  }
};

inline bool OpenSsh(Input &input) {
  for (const char byte : "openssh-key-v1")
    if (input.Byte() != byte)
      return false;
  std::string cipher, kdf;
  uint32_t options, salt, rounds, keys;
  return input.SshName(cipher) && cipher != "none" && input.SshName(kdf) &&
         kdf == "bcrypt" && input.UInt(options) && input.UInt(salt) &&
         salt > 0 && salt <= 64 && options == salt + 8 && input.Skip(salt) &&
         input.UInt(rounds) && rounds > 0 && input.UInt(keys) && keys > 0;
}

inline bool Pkcs8(Input &input) {
  // EncryptedPrivateKeyInfo begins with a SEQUENCE of the encryption
  // AlgorithmIdentifier and the encrypted data. Its OID is not key material.
  uint32_t whole, algorithm, oid;
  return input.Byte() == 0x30 && input.DerSize(whole) &&
         input.Byte() == 0x30 && input.DerSize(algorithm) &&
         algorithm < whole && input.Byte() == 0x06 && input.DerSize(oid) &&
         oid > 0 && oid <= 32 && oid < algorithm && input.Skip(oid);
}

inline bool LegacyPem(Input &input) {
  std::string line;
  if (!input.Line(line) || line != "Proc-Type: 4,ENCRYPTED" ||
      !input.Line(line) || line.compare(0, 10, "DEK-Info: ") != 0)
    return false;
  const auto comma = line.find(',', 10);
  if (comma == std::string::npos || comma == 10)
    return false;
  if (line.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-", 10) !=
      comma)
    return false;
  const auto ivLength = line.size() - comma - 1;
  return (ivLength == 16 || ivLength == 32) &&
         line.find_first_not_of("0123456789abcdefABCDEF", comma + 1) ==
             std::string::npos && input.Line(line) && line.empty() &&
         input.Byte() >= 0;
}
} // namespace SshKeyHeader

inline bool SshKeyNeedsPassphrase(const std::string &path) {
  // The filename is local metadata; no filename or key bytes are logged.
  SshKeyHeader::Input input(path);
  std::string marker;
  if (!input.Line(marker))
    return false;
  if (marker == "-----BEGIN OPENSSH PRIVATE KEY-----")
    return SshKeyHeader::OpenSsh(input);
  if (marker == "-----BEGIN ENCRYPTED PRIVATE KEY-----")
    return SshKeyHeader::Pkcs8(input);
  if (marker == "-----BEGIN RSA PRIVATE KEY-----" ||
      marker == "-----BEGIN DSA PRIVATE KEY-----" ||
      marker == "-----BEGIN EC PRIVATE KEY-----")
    return SshKeyHeader::LegacyPem(input);
  return false;
}
} // namespace Listi
