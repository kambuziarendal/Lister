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
  int64 Enqueue(const Listi::Job &);
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
  bool Pause(int64 operation);
  bool CancelOperation(int64 operation);
  // With an ID, cancel all only if that job is still running.
  bool Cancel(int64 expectedOperation = 0);
  bool Answer(Listi::Decision, int64 request);

private:
  struct QueuedJob {
    int64 operation = 0;
    Listi::Job job;
  };
  void Loop();
  void Publish(const BMessage &);
  void PublishCancelled(const QueuedJob &);
  bool Proceed();
  Listi::Decision Ask(const std::string &);
  class ProgressView *view;
  BMessenger main, display;
  std::thread thread;
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<QueuedJob> jobs;
  std::vector<BMessage> events;
  std::unique_ptr<BMessageRunner> eventTimer;
  std::atomic<int> count{0};
  bool paused = false, cancel = false, stopping = false;
  int answer = -1;
  int64 conflictSerial = 0, operationSerial = 0;
  int64 runningOperation = 0;
  bool awaitingAnswer = false;
};
