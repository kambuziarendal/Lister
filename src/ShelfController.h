// SPDX-License-Identifier: MIT
#pragma once

#include "Shelf.h"
#include <Messenger.h>
#include <array>
#include <map>
#include <memory>
#include <string>

class OperationQueue;
class Pane;

namespace Listi {

// Window-looper owner of the shared Shelf and its child window messengers.
class ShelfController {
public:
  ShelfController(BMessenger owner, const std::array<Pane *, 2> &panes);

  const std::shared_ptr<Shelf> &Model() const { return fShelf; }
  static bool CanOpenFlatView(const char *path);
  bool Handle(const BMessage &message, Pane &active, OperationQueue &queue);
  void Add(Pane &active,
           const std::map<std::string, NodeIdentity> &expected = {});
  void Refresh();

private:
  void RefreshSidebars();
  void OpenFlatViewFor(const char *root);

  BMessenger fOwner;
  const std::array<Pane *, 2> &fPanes;
  std::shared_ptr<Shelf> fShelf = std::make_shared<Shelf>();
  BMessenger fShelfWindow;
  std::map<std::string, BMessenger> fFlatViews;
};

} // namespace Listi
