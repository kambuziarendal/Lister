// SPDX-License-Identifier: MIT
#pragma once
#include "Common.h"
#include "FileOperation.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
class BView;
class BMessageRunner;
class OperationQueue {
public:
  explicit OperationQueue(BMessenger main);
  ~OperationQueue();
  void Enqueue(const Listi::Job &);
  void SetCompact(bool);
  void Show();
  void Toggle();
  void Collapse();
  void ErrorInfo(const char *);
  BView *View();
  void Start();
  // Critical state is retained until the owning UI consumes it. Notifications are
  // only wakeups; a lost message must not strand a clipboard lease.
  std::vector<BMessage> TakeEvents();
  void PresentEvent(BMessage &);
  bool Busy() const { return count.load() > 0; }
  void Pause();
  void Cancel();
  bool Answer(Listi::Decision, int64 request);

private:
  void Loop();
  void Publish(const BMessage &);
  bool Proceed();
  Listi::Decision Ask(const std::string &);
  class ProgressView *view;
  BMessenger main, display;
  std::thread thread;
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Listi::Job> jobs;
  std::vector<BMessage> events;
  std::unique_ptr<BMessageRunner> eventTimer;
  std::atomic<int> count{0};
  bool paused = false, cancel = false, stopping = false;
  int answer = -1;
  int64 conflictSerial = 0, operationSerial = 0;
  bool awaitingAnswer = false;
};
