// SPDX-License-Identifier: MIT
#pragma once
#include "Common.h"
#include <Button.h>
#include <Looper.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>
class FileList;
class Pane : public BView {
public:
  Pane(int32 id, const Preferences &);
  ~Pane();
  void AttachedToWindow() override;
  void MessageReceived(BMessage *) override;
  void Navigate(const char *path);
  void Reload();
  void SetSource(bool);
  void Apply(const Preferences &);
  void Select(int mode);
  std::vector<std::string> Selected() const;
  const char *Path() const { return path.String(); }
  void FocusList();
  void Activate();
  bool IsScanning() const { return scanning; }
  void SendSelection();

private:
  int32 id;
  Preferences prefs;
  BString path = "/boot/home";
  FileList *list;
  BTextControl *edit;
  BStringView *status;
  BButton *role;
  BLooper *scanner = nullptr;
  std::shared_ptr<std::atomic<int64>> generation;
  int64 serial = 0;
  bool scanning = false, refreshPending = false;
  std::vector<std::string> savedSelection;
};
