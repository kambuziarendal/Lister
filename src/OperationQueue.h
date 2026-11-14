// SPDX-License-Identifier: MIT
#pragma once
#include "Common.h"
#include "FileOperation.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
class OperationQueue {
public:
  explicit OperationQueue(BMessenger main);
  ~OperationQueue();
  void Enqueue(const Lister::Job &);
  void Show();
  bool Busy() const { return count.load() > 0; }
  void Pause();
  void Cancel();
  void Answer(Lister::Decision);

private:
  void Loop();
  bool Proceed();
  Lister::Decision Ask(const std::string &);
  class ProgressWindow *window;
  BMessenger main, display;
  std::thread thread;
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Lister::Job> jobs;
  std::atomic<int> count{0};
  bool paused = false, cancel = false, stopping = false;
  int answer = -1;
};
