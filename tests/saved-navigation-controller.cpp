// SPDX-License-Identifier: MIT
#include "Common.h"
#include "SavedNavigationController.h"
#include <Application.h>
#include <Looper.h>
#include <OS.h>
#include <cassert>
#include <stdexcept>

using namespace Listi;

// Capture one request at a time without opening the user's settings files.
class SaveRequests : public BLooper {
public:
  SaveRequests() : BLooper("test save requests") { assert(ready >= B_OK); }
  ~SaveRequests() override { delete_sem(ready); }

  void MessageReceived(BMessage *message) override {
    last = *message;
    release_sem(ready);
  }

  BMessage Next() {
    assert(acquire_sem_etc(ready, 1, B_RELATIVE_TIMEOUT, 5000000) == B_OK);
    assert(Lock());
    BMessage request(last);
    Unlock();
    return request;
  }

private:
  sem_id ready = create_sem(0, "test save queued");
  BMessage last;
};

int main() {
  BApplication app("application/x-vnd.Listi-SavedNavigationTest");
  auto *requests = new SaveRequests;
  requests->Run();
  BMessenger service(requests), author(&app);
  SavedNavigationController state(service, {}, true, {}, true);

  BMessage place(kPlaceAccepted);
  place.AddInt32("action", 0);
  place.AddString("path", "/boot/home/work");
  place.AddString("name", "Work");
  state.EditPlace(place);
  const auto savePlace = requests->Next();
  assert(savePlace.what == kSavePlaces);
  assert(SavedPlaces::FromMessage(savePlace).Find("/boot/home/work"));
  assert(!state.CanEditPlaces() && state.Saving());
  assert(!state.Places().Find("/boot/home/work"));
  BMessage placeReply(kPlacesSaved);
  const auto serial = savePlace.GetInt64("serial", -1);
  assert(serial > 0);
  placeReply.AddInt64("serial", serial - 1);
  assert(!state.FinishPlaceSave(placeReply) && !state.CanEditPlaces());

  // The stores are independent. A pending place must not block a search.
  BMessage search(kSearchEdit);
  search.AddInt64("revision", 0);
  search.AddString("name", "Notes");
  search.AddString("root", "/boot/home");
  search.AddString("expression", "meeting");
  assert(!state.EditSearch(search, author));
  const auto saveSearch = requests->Next();
  assert(saveSearch.what == kSaveSearches);
  assert(saveSearch.GetInt64("revision", -1) == 0);
  assert(SavedSearches::FromMessage(saveSearch).Find("Notes"));
  assert(state.SearchStore().GetBool("saving", false));
  assert(!SavedSearches::FromMessage(state.SearchStore()).Find("Notes"));
  auto rejected = state.EditSearch(search, service);
  assert(rejected && rejected->author == service && rejected->message.HasString("error"));

  placeReply.ReplaceInt64("serial", serial);
  assert(state.FinishPlaceSave(placeReply));
  assert(state.CanEditPlaces() && state.Places().Find("/boot/home/work"));
  assert(state.Saving()); // The search is still pending.
  assert(!state.FinishPlaceSave(placeReply));

  BMessage searchReply(kSearchesSaved);
  searchReply.AddInt64("revision", 1);
  searchReply.AddInt64("serial", saveSearch.GetInt64("serial", -1));
  assert(!state.FinishSearchSave(searchReply) && state.Saving());
  searchReply.ReplaceInt64("revision", 0);
  searchReply.AddString("error", "Store unavailable");
  auto failed = state.FinishSearchSave(searchReply);
  assert(failed && failed->author == author && failed->message.HasString("error"));
  assert(!state.Saving() && state.SearchStore().GetInt64("revision", -1) == 0);
  assert(!SavedSearches::FromMessage(state.SearchStore()).Find("Notes"));

  BMessage retry(search);
  retry.ReplaceString("name", "  Retry  ");
  assert(!state.EditSearch(retry, service));
  const auto nextSave = requests->Next();
  assert(nextSave.what == kSaveSearches);
  // A failed attempt does not advance the store revision. Its old reply must
  // not settle a later writer's request, even at the same revision.
  assert(!state.FinishSearchSave(searchReply) && state.Saving());
  const auto nextSerial = nextSave.GetInt64("serial", -1);
  assert(nextSerial > searchReply.GetInt64("serial", -1));
  searchReply.ReplaceInt64("serial", nextSerial);
  searchReply.RemoveName("error");
  auto committed = state.FinishSearchSave(searchReply);
  assert(committed && committed->author == service);
  assert(committed->message.GetBool("committed", false));
  assert(std::string(committed->message.GetString("chosen", "")) == "Retry");
  assert(committed->message.GetInt64("revision", -1) == 1);
  const auto stored = SavedSearches::FromMessage(committed->message);
  assert(stored.Find("Retry") && !stored.Find("Notes"));
  assert(!state.FinishSearchSave(searchReply));
  rejected = state.EditSearch(search, author); // editor still has revision 0
  assert(rejected && rejected->message.HasString("error") && !state.Saving());

  // Only the requesting editor gets the rejection. The other search windows
  // receive the same committed list without changing their own selection.
  auto *standalone = new SaveRequests;
  auto *left = new SaveRequests;
  auto *right = new SaveRequests;
  for (auto *editor : {standalone, left, right})
    editor->Run();
  search.AddMessenger("searchEditor", BMessenger(right));
  state.HandleSearchMessage(search, BMessenger(standalone),
                            {BMessenger(left), BMessenger(right)});
  for (auto *editor : {right, standalone, left}) {
    auto update = editor->Next();
    assert(update.what == kSearchStore);
    assert(update.HasString("error") == (editor == right));
    assert(!update.HasString("chosen") && !update.GetBool("saving", true));
    assert(SavedSearches::FromMessage(update).Find("Retry"));
    assert(editor->Lock());
    editor->Quit();
  }

  // A failure to enqueue must not keep quit blocked or publish an unsaved edit.
  SavedNavigationController disconnected(BMessenger{}, {}, true, {}, true);
  try {
    disconnected.EditPlace(place);
    assert(false);
  } catch (const std::runtime_error &) {}
  assert(disconnected.CanEditPlaces() && disconnected.Places().Items().empty());
  rejected = disconnected.EditSearch(search, author);
  assert(rejected && rejected->message.HasString("error") && !disconnected.Saving());

  assert(requests->Lock());
  requests->Quit();
}
