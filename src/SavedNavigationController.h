// SPDX-License-Identifier: MIT
#pragma once
#include "SavedPlaces.h"
#include "SavedSearches.h"
#include <Messenger.h>
#include <optional>

namespace Listi {

// Window-looper state for the two navigation stores. Only the service writes to
// disk; readers see the last committed list until its matching reply arrives.
class SavedNavigationController {
public:
  SavedNavigationController(BMessenger service, const SavedPlaces &places,
                            bool placesWritable, const SavedSearches &searches,
                            bool searchesWritable);

  bool Saving() const { return pendingPlaces || pendingSearch; }
  bool CanEditPlaces() const { return placesWritable && !pendingPlaces; }
  const SavedPlaces &Places() const { return places; }

  void EditPlace(const BMessage &edit);
  bool FinishPlaceSave(const BMessage &reply);

  struct SearchUpdate {
    BMessage message;
    BMessenger author;
  };
  BMessage SearchStore() const;
  std::optional<SearchUpdate> EditSearch(const BMessage &edit, BMessenger author);
  std::optional<SearchUpdate> FinishSearchSave(const BMessage &reply);

private:
  BMessenger service;
  SavedPlaces places;
  bool placesWritable;
  int64 placesSerial = 0;
  std::optional<SavedPlaces> pendingPlaces;

  SavedSearches searches;
  bool searchesWritable;
  int64 searchesRevision = 0;
  struct PendingSearch {
    SavedSearches next;
    std::string chosen;
    BMessenger author;
  };
  std::optional<PendingSearch> pendingSearch;
};

} // namespace Listi
