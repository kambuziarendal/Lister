// SPDX-License-Identifier: MIT
#include "SavedNavigationController.h"
#include "Common.h"
#include <stdexcept>
#include <utility>

namespace Listi {

SavedNavigationController::SavedNavigationController(
    BMessenger service, const SavedPlaces &places, bool placesWritable,
    const SavedSearches &searches, bool searchesWritable)
    : service(service), places(places), placesWritable(placesWritable),
      searches(searches), searchesWritable(searchesWritable) {}

void SavedNavigationController::EditPlace(const BMessage &edit) {
  if (!CanEditPlaces())
    throw std::runtime_error("Saved places are unavailable or still being saved.");

  auto next = places;
  const char *path = edit.GetString("path", "");
  const char *name = edit.GetString("name", "");
  switch (edit.GetInt32("action", -1)) {
  case 0:
    next.Add(name, path);
    break;
  case 1:
    next.Rename(path, name);
    break;
  case 2:
    next.Remove(path);
    break;
  }
  auto save = next.Message();
  save.what = kSavePlaces;
  save.AddInt64("serial", ++placesSerial);
  pendingPlaces = std::move(next);
  if (service.SendMessage(&save) != B_OK) {
    pendingPlaces.reset();
    throw std::runtime_error("Could not queue saved places.");
  }
}

bool SavedNavigationController::FinishPlaceSave(const BMessage &reply) {
  if (!pendingPlaces || reply.GetInt64("serial", -1) != placesSerial)
    return false;
  if (!reply.HasString("error"))
    places = std::move(*pendingPlaces);
  pendingPlaces.reset();
  return true;
}

BMessage SavedNavigationController::SearchStore() const {
  auto message = searches.Message();
  message.what = kSearchStore;
  message.AddInt64("revision", searchesRevision);
  message.AddBool("writable", searchesWritable);
  message.AddBool("saving", pendingSearch.has_value());
  return message;
}

std::optional<SavedNavigationController::SearchUpdate>
SavedNavigationController::EditSearch(const BMessage &edit, BMessenger author) {
  try {
    if (!searchesWritable)
      throw std::runtime_error(
          "Saved searches could not be loaded. Their file is preserved; changes are disabled.");
    if (pendingSearch || edit.GetInt64("revision", -1) != searchesRevision)
      throw std::runtime_error(
          "The saved-search list changed or is being written. Please try again.");

    auto next = searches;
    SavedSearch item;
    item.name = edit.GetString("name", "");
    item.request.root = edit.GetString("root", "");
    item.request.expression = edit.GetString("expression", "");
    item.request.filters = ReadSearchFilters(edit);
    item.request.predicate = edit.GetBool("predicate", false);
    item.request.includeTrash = edit.GetBool("trash", false);
    item.live = edit.GetBool("live", false);
    const bool remove = edit.GetBool("remove", false);
    if (remove)
      next.Remove(item.name);
    else
      next.Put(item, edit.GetBool("replace", false));

    auto request = next.Message();
    request.what = kSaveSearches;
    request.AddInt64("revision", searchesRevision);
    pendingSearch = PendingSearch{std::move(next), remove ? "" : item.name, author};
    if (service.SendMessage(&request) != B_OK) {
      pendingSearch.reset();
      throw std::runtime_error("Could not queue saved search.");
    }
    return std::nullopt;
  } catch (const std::exception &e) {
    auto reply = SearchStore();
    reply.AddString("error", e.what());
    return SearchUpdate{reply, author};
  }
}

std::optional<SavedNavigationController::SearchUpdate>
SavedNavigationController::FinishSearchSave(const BMessage &result) {
  if (!pendingSearch || result.GetInt64("revision", -1) != searchesRevision)
    return std::nullopt;

  auto saved = std::move(*pendingSearch);
  pendingSearch.reset();
  if (!result.HasString("error")) {
    searches = std::move(saved.next);
    ++searchesRevision;
  }
  auto reply = SearchStore();
  if (result.HasString("error"))
    reply.AddString("error", result.GetString("error", "Save failed."));
  else {
    reply.AddBool("committed", true);
    reply.AddString("chosen", saved.chosen.c_str());
  }
  return SearchUpdate{reply, saved.author};
}

} // namespace Listi
