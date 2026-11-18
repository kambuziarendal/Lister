// SPDX-License-Identifier: MIT
#pragma once
#include <GroupView.h>
#include <map>
#include <string>
class OperationQueue;
class BPopUpMenu;
class BMenuItem;
class BButton;
class BTextView;

// UI selection is independent of the job currently using the worker.
class JobResultsView : public BGroupView {
public:
  explicit JobResultsView(OperationQueue *);
  void Accepted(int64 id, const char *title);
  void Started(int64 id);
  void Finished(const BMessage &event);
  void AppendActivity(const std::string &line);
  void AttachedToWindow() override;
  void MessageReceived(BMessage *) override;

private:
  struct Row {
    BMenuItem *item;
    std::string title;
    bool waiting;
    bool finished;
  };
  void SetState(int64 id, const char *state, bool waiting);
  void Display();
  void ClearFinished();
  OperationQueue *queue;
  BPopUpMenu *jobs;
  BButton *previous, *next, *cancelWaiting, *clear;
  BTextView *text;
  std::map<int64, Row> rows;
  std::string activity;
  int64 selected = 0;
  size_t first = 0;
};
