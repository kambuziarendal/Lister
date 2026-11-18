// SPDX-License-Identifier: MIT
#include "TerminalSession.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace Listi {
namespace {

void WindowSize(int fd, int rows, int columns) {
  winsize size{};
  size.ws_row = rows;
  size.ws_col = columns;
  ioctl(fd, TIOCSWINSZ, &size, sizeof(size));
}

[[noreturn]] void ChildError(int fd) {
  const int error = errno;
  write(fd, &error, sizeof(error));
  _exit(126);
}

void StartShell(int master, const char *slaveName, int errorFd,
                const char *directory, int rows, int columns,
                char *const environment[]) {
  close(master);
  if (setsid() < 0)
    ChildError(errorFd);
  const int slave = open(slaveName, O_RDWR);
  if (slave < 0)
    ChildError(errorFd);
#ifdef TIOCSCTTY
  if (ioctl(slave, TIOCSCTTY, 0) < 0)
    ChildError(errorFd);
#endif
  for (int destination = 0; destination <= 2; ++destination) {
    if (dup2(slave, destination) < 0)
      ChildError(errorFd);
  }
  if (slave > 2)
    close(slave);

  termios attributes{};
  if (tcgetattr(STDIN_FILENO, &attributes) < 0)
    ChildError(errorFd);
  attributes.c_iflag |= ICRNL;
  attributes.c_oflag |= OPOST | ONLCR;
  attributes.c_lflag |= ISIG | ICANON | ECHO | ECHOE;
  attributes.c_cc[VINTR] = 3;
  attributes.c_cc[VQUIT] = 28;
  attributes.c_cc[VERASE] = 127;
  attributes.c_cc[VKILL] = 21;
  attributes.c_cc[VEOF] = 4;
  attributes.c_cc[VSUSP] = 26;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &attributes) < 0 ||
      tcsetpgrp(STDIN_FILENO, getpgrp()) < 0 || chdir(directory) < 0)
    ChildError(errorFd);
  WindowSize(STDIN_FILENO, rows, columns);

  for (int signum : {SIGCHLD, SIGHUP, SIGQUIT, SIGTERM, SIGINT, SIGTTOU,
                     SIGTTIN, SIGTSTP, SIGPIPE}) {
    struct sigaction action {};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigaction(signum, &action, nullptr);
  }
  sigset_t mask;
  sigemptyset(&mask);
  sigprocmask(SIG_SETMASK, &mask, nullptr);
  char bash[] = "/bin/bash";
  char interactive[] = "-i";
  char *const arguments[] = {bash, interactive, nullptr};
  execve(bash, arguments, environment);
  ChildError(errorFd);
}

} // namespace

TerminalSession::~TerminalSession() { Stop(); }

int TerminalSession::Start(const char *directory, int rows, int columns) {
  if (master >= 0)
    return EBUSY;
  char term[] = "TERM=xterm-256color";
  char color[] = "COLORTERM=truecolor";
  std::vector<char *> environment{term, color};
  for (char **entry = environ; *entry; ++entry)
    if (strncmp(*entry, "TERM=", 5) && strncmp(*entry, "COLORTERM=", 10))
      environment.push_back(*entry);
  environment.push_back(nullptr);

  master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (master < 0)
    return errno;
  const char *slave = nullptr;
  if (grantpt(master) != 0 || unlockpt(master) != 0 ||
      !(slave = ptsname(master))) {
    const int error = errno;
    Stop();
    return error;
  }
  const std::string slaveName(slave);
  int statusPipe[2];
  if (pipe(statusPipe) < 0) {
    const int error = errno;
    Stop();
    return error;
  }
  if (statusPipe[1] <= STDERR_FILENO) {
    const int descriptor = fcntl(statusPipe[1], F_DUPFD, STDERR_FILENO + 1);
    const int error = errno;
    close(statusPipe[1]);
    if (descriptor < 0) {
      close(statusPipe[0]);
      Stop();
      return error;
    }
    statusPipe[1] = descriptor;
  }
  int error = 0;
  if (fcntl(statusPipe[0], F_SETFD, FD_CLOEXEC) < 0 ||
      fcntl(statusPipe[1], F_SETFD, FD_CLOEXEC) < 0 ||
      fcntl(master, F_SETFL, O_NONBLOCK) < 0) {
    error = errno;
  } else {
    shell = fork();
    if (shell == 0) {
      close(statusPipe[0]);
      StartShell(master, slaveName.c_str(), statusPipe[1], directory, rows,
                 columns, environment.data());
    }
    if (shell < 0)
      error = errno;
  }
  close(statusPipe[1]);
  if (!error) {
    pollfd ready{statusPipe[0], POLLIN, 0};
    const int result = poll(&ready, 1, 1500);
    if (result > 0) {
      int childError = 0;
      const ssize_t received =
          read(statusPipe[0], &childError, sizeof(childError));
      if (received != 0)
        error = received == sizeof(childError) ? childError : EIO;
    } else {
      error = result == 0 ? ETIMEDOUT : errno;
    }
  }
  close(statusPipe[0]);
  if (error)
    Stop();
  return error;
}

void TerminalSession::Resize(int rows, int columns) {
  if (master >= 0)
    WindowSize(master, rows, columns);
}

bool TerminalSession::Reap() {
  if (shell > 0) {
    const pid_t result = waitpid(shell, nullptr, WNOHANG);
    if (result == shell || (result < 0 && errno == ECHILD))
      shell = -1;
  }
  return shell < 0;
}

void TerminalSession::Stop() {
  if (master >= 0) {
    const pid_t foreground = tcgetpgrp(master);
    if (foreground > 0 && foreground != shell && foreground != getpgrp())
      kill(-foreground, SIGHUP);
    close(master);
    master = -1;
  }
  if (Reap())
    return;
  kill(-shell, SIGHUP);
  kill(shell, SIGHUP);
  for (int tries = 0; tries < 10 && !Reap(); ++tries)
    usleep(10000);
  if (Reap())
    return;
  kill(shell, SIGKILL);
  while (waitpid(shell, nullptr, 0) < 0 && errno == EINTR) {
  }
  shell = -1;
}

} // namespace Listi
