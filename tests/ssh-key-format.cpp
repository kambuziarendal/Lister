// SPDX-License-Identifier: MIT
#include "SshKeyFormat.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

int main() {
  char path[] = "/tmp/Listi-key-header-XXXXXX";
  const int file = mkstemp(path);
  if (file < 0)
    return 2;
  close(file);
  int failures = 0;
  const auto check = [&](const std::string &header, bool encrypted,
                         const char *label) {
    std::ofstream fixture(path, std::ios::binary);
    fixture << header;
    fixture.close();
    const bool ok = fixture.good() &&
                    Listi::SshKeyNeedsPassphrase(path) == encrypted;
    std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
    failures += !ok;
  };

  const std::string ssh = "-----BEGIN OPENSSH PRIVATE KEY-----\n";
  // Header only: cipher, KDF, salt, rounds, and number of keys. No key bytes.
  const std::string protectedSsh =
      "b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAA"
      "ABAAAQIDBAUGBwgJCgsMDQ4PAAAAEAAAAAE=";
  check(ssh + protectedSsh.substr(0, 60) + "\r\n" + protectedSsh.substr(60),
        true, "wrap and CRLF do not obscure the encrypted OpenSSH header");
  check(ssh + "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAAB",
        false, "clear OpenSSH uses cipher none");
  check(ssh + "b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAA"
              "ABAAAQIDBAUGBwgJCgsMDQ4PAAAAAAAAAAE=",
        false, "invalid bcrypt options do not indicate encryption");
  check(ssh + protectedSsh.substr(0, 48), false,
        "truncated OpenSSH header is not a positive match");
  check(ssh + "b3BlbnNzaC1rZXktdjEAAAAA////" + protectedSsh, false,
        "invalid OpenSSH cipher length is rejected");
  check(ssh + std::string(1200, '\n') + protectedSsh, false,
        "base64 scanning is bounded even across whitespace");

  const std::string pkcs8 = "-----BEGIN ENCRYPTED PRIVATE KEY-----\n";
  check(pkcs8 + "MBQwDQYJKoZIhvcNAQELBQAEAwECAw==", true,
        "encrypted PKCS#8 envelope is recognized");
  check("-----BEGIN PRIVATE KEY-----\nMBQwDQYJKoZIhvcNAQELBQAEAwECAw==",
        false, "clear PKCS#8 does not prompt");
  check(pkcs8 + "not DER\n", false,
        "encrypted PKCS#8 marker is not enough without a DER header");

  const std::string pem = "-----BEGIN RSA PRIVATE KEY-----\r\n";
  const std::string proc = "Proc-Type: 4,ENCRYPTED\r\n";
  check(pem + proc + "DEK-Info: AES-256-CBC,114422336655AABBCCDDEEFF00112934\r\n"
                     "\r\nMBQw\r\n",
        true, "encrypted legacy PEM headers are recognized");
  check(pem + "MBQwDQYJKoZIhvcNAQELBQAEAwECAw==", false,
        "clear legacy PEM does not prompt");
  check(pem + proc + "DEK-Info: AES-256-CBC,XX\n\nMBQw", false,
        "invalid legacy PEM IV is not a positive match");
  check(pem + proc + "DEK-Info: " + std::string(200, 'A'), false,
        "oversize legacy PEM header is rejected");

  unlink(path);
  if (Listi::SshKeyNeedsPassphrase(path)) {
    std::cout << "FAIL missing key cannot indicate encryption\n";
    ++failures;
  }
  return failures ? 1 : 0;
}
