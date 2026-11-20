// SPDX-License-Identifier: MIT
#pragma once

#include <Message.h>
#include <Messenger.h>
#include <String.h>
#include <memory>
#include <string>

class BFilePanel;
class Pane;

namespace Listi {
struct Job;

constexpr uint32 kArchiveAddChosen = 'aadc', kArchiveEditAccepted = 'aeda';

struct ArchivePrompt {
  BString text;
  const char *button;
  BMessage accepted;
};

class ArchiveActions {
public:
  ArchiveActions();
  ~ArchiveActions();

  void ShowJobWindow(bool extract, const Pane &source, const Pane &destination,
                     BMessenger owner);
  void ShowAddPicker(const Pane &source, BMessenger owner);
  static Job ReadAdditions(const BMessage &message);
  static ArchivePrompt ConfirmAddition(const Job &job,
                                       const std::string &lease);
  static ArchivePrompt ConfirmRemoval(const Pane &source);

private:
  std::unique_ptr<BFilePanel> archiveAddPicker;
  BMessenger archiveWindow;
};
}
