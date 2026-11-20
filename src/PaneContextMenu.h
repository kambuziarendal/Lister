// SPDX-License-Identifier: MIT
#pragma once

#include "FileOperation.h"
#include <Message.h>

class Pane;

namespace Listi {

class SavedPlaces;

struct PaneContextMenuResult {
  BMessage command{uint32(0)};
  std::map<std::string, NodeIdentity> shelfSources;
};

// Show the popup and validate its choice against the live pane state before
// returning a window command. The caller must dispatch the result synchronously.
PaneContextMenuResult RunPaneContextMenu(
    const BMessage &request, Pane &pane, Pane &other, int owner,
    bool singlePane, bool dual, bool rememberView,
    const SavedPlaces &places, bool placesWritable);

} // namespace Listi
