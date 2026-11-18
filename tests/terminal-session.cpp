// SPDX-License-Identifier: MIT
#include "TerminalSession.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <memory>
#include <string>
#include <unistd.h>

static bool ReadUntil(Listi::TerminalSession &session, const char *expected) {
  std::string output;
  for (int attempt = 0; attempt < 120; ++attempt) {
    pollfd ready{session.Descriptor(), POLLIN, 0};
    if (poll(&ready, 1, 25) <= 0)
      continue;
    char buffer[4096];
    const ssize_t size = read(ready.fd, buffer, sizeof(buffer));
    if (size > 0) {
      output.append(buffer, size);
      if (output.find(expected) != std::string::npos)
        return true;
    }
  }
  fprintf(stderr, "Expected %s in terminal output:\n%s\n", expected,
          output.c_str());
  return false;
}

static void Send(Listi::TerminalSession &session, const char *bytes) {
  const size_t size = strlen(bytes);
  assert(write(session.Descriptor(), bytes, size) == ssize_t(size));
}

int main() {
  char directory[] = "/tmp/Listi CLI ' test.XXXXXX";
  assert(mkdtemp(directory));
  const std::unique_ptr<char, decltype(&free)> physical(
      realpath(directory, nullptr), free);
  assert(physical);
  Listi::TerminalSession session;
  assert(session.Start("/not/a/Listi/test/directory", 12, 60) == ENOENT);
  const int status = session.Start(directory, 12, 60);
  if (status)
    fprintf(stderr, "Could not start test shell: %s\n", strerror(status));
  assert(status == 0);
  Send(session, "test -t 0 && test -t 1 && printf 'TTY-%s\\n' \"$(pwd -P)\"\r");
  assert(ReadUntil(session, (std::string("TTY-") + physical.get()).c_str()));
  Send(session, "printf 'SIZE-%s\\n' \"$(stty size)\"\r");
  assert(ReadUntil(session, "SIZE-12 60"));
  session.Resize(24, 88);
  Send(session, "printf 'SIZE-%s\\n' \"$(stty size)\"\r");
  assert(ReadUntil(session, "SIZE-24 88"));
  Send(session, "printf 'RUN-%s\\n' OK; sleep 60\r");
  assert(ReadUntil(session, "RUN-OK"));
  usleep(50000);
  Send(session, "\003");
  Send(session, "printf 'INTERRUPT-%s\\n' OK\r");
  assert(ReadUntil(session, "INTERRUPT-OK"));
  Send(session, "exit\r");
  for (int attempt = 0; attempt < 120 && !session.Reap(); ++attempt)
    usleep(25000);
  assert(session.Reap());
  session.Stop();
  assert(session.Descriptor() < 0);
  assert(session.Start(directory, 12, 60) == 0);
  session.Stop();
  assert(session.Reap() && session.Descriptor() < 0);
  assert(rmdir(directory) == 0);
  puts("Interactive PTY: working directory, size, interrupt, exit, reopen OK");
}
