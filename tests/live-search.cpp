// SPDX-License-Identifier: MIT
#include "SearchWorker.h"
#include "FileSearch.h"
#include <Application.h>
#include <Node.h>
#include <String.h>
#include <fs_index.h>
#include <fstream>
#include <iostream>
#include <functional>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
using namespace Listi;
class Receiver : public BLooper {
public:
  sem_id ready = create_sem(0, "search result");
  std::vector<SearchHit> rows, pending;
  int64 serial = 0, revision = 0, events = 0;
  bool live = false; std::string error;
  ~Receiver() { delete_sem(ready); }
  void MessageReceived(BMessage *m) override {
    if (m->what == kSearchBegin) { pending.clear(); }
    else if (m->what == kSearchRows) {
      BMessage hit;
      for (int32 i = 0; m->FindMessage("hit", i, &hit) == B_OK; ++i)
        pending.push_back({hit.GetString("path", ""), hit.GetInt64("device", -1), hit.GetInt64("node", -1)});
    } else if (m->what == kSearchDone) {
      rows = pending; serial = m->GetInt64("serial", -1); revision = m->GetInt64("revision", -1);
      live = m->GetBool("live", false); error = m->GetString("error", ""); events = m->GetInt64("events", 0);
      release_sem(ready);
    } else BLooper::MessageReceived(m);
  }
  bool Await(const std::function<bool()> &condition, bigtime_t timeout = 8000000) {
    bigtime_t deadline = system_time() + timeout;
    do {
      Lock(); bool ok = condition(); Unlock(); if (ok) return true;
    } while (acquire_sem_etc(ready, 1, B_ABSOLUTE_TIMEOUT, deadline) == B_OK);
    return false;
  }
};
int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-LiveSearchTest");
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) { ++checks; failures += !ok; std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl; };
  char pattern[] = "/boot/home/listi-live-test-XXXXXX"; char *created = mkdtemp(pattern); if (!created) return 2;
  std::string base = created, scope = base + "/scope", outside = base + "/outside";
  mkdir(scope.c_str(), 0700); mkdir(outside.c_str(), 0700);
  auto write = [](const std::string &p) { std::ofstream(p) << "synthetic live content"; };
  auto receiver = new Receiver; receiver->Run();
  auto token = std::make_shared<std::atomic<int64>>(0);
  BLooper *worker = NewSearchWorker(BMessenger(receiver), token); worker->Run();
  int64 generation = 0;
  auto start = [&](const std::string &expression, bool live = true) {
    token->store(++generation);
    BMessage m(kSearchStart); m.AddInt64("serial", generation); m.AddString("root", scope.c_str());
    m.AddString("expression", expression.c_str()); m.AddBool("predicate", true); m.AddBool("live", live);
    worker->PostMessage(&m);
  };
  auto count = [&](size_t n) { return receiver->Await([&] { return receiver->serial == generation && receiver->live && receiver->error.empty() && receiver->rows.size() == n; }); };
  start("name==\"*.live\""); check(count(0), "initial empty native live query");
  write(scope + "/first.live"); check(count(1), "new file enters live results");
  check(receiver->Await([&] { return receiver->events > 0; }), "real B_QUERY_UPDATE notifications received");
  rename((scope + "/first.live").c_str(), (scope + "/first.no").c_str()); check(count(0), "rename out of predicate removes result");
  rename((scope + "/first.no").c_str(), (scope + "/first.live").c_str()); check(count(1), "rename into predicate adds result");
  unlink((scope + "/first.live").c_str()); check(count(0), "deleted file leaves results");
  mkdir((outside + "/branch").c_str(), 0700); write(outside + "/branch/nested.live");
  rename((outside + "/branch").c_str(), (scope + "/branch").c_str()); check(count(1), "ancestor folder moved into scope reconciles descendants");
  rename((scope + "/branch").c_str(), (outside + "/branch").c_str()); check(count(0), "ancestor folder moved outside scope removes descendants");
  write(scope + "/same.live"); check(count(1), "identity fixture appears");
  receiver->Lock(); SearchHit old = receiver->rows.empty() ? SearchHit{} : receiver->rows[0]; receiver->Unlock();
  rename((scope + "/same.live").c_str(), (scope + "/old.no").c_str()); write(scope + "/same.live");
  check(receiver->Await([&] { return receiver->rows.size() == 1 && receiver->rows[0].node != old.node; }), "replacement at same path has new identity");
  check(!SameSearchEntry(old), "old selection identity rejected");
  struct stat st; stat(scope.c_str(), &st); std::string index = "Listi:live-test-" + std::to_string(getpid());
  bool owned = fs_create_index(st.st_dev, index.c_str(), B_STRING_TYPE, 0) == 0; check(owned, "owned native test attribute index");
  start(index + "==\"yes\""); check(count(0), "attribute live query starts empty");
  BNode node((scope + "/same.live").c_str()); BString yes("yes"), no("no");
  node.WriteAttrString(index.c_str(), &yes); check(count(1), "attribute change enters predicate");
  node.WriteAttrString(index.c_str(), &no); check(count(0), "attribute change leaves predicate");
  node.RemoveAttr(index.c_str());
  token->store(++generation); BMessage stop(kSearchStop); stop.AddInt64("serial", generation - 1); worker->PostMessage(&stop);
  start("name==\"*.live\"", false);
  check(receiver->Await([&] { return receiver->serial == generation && !receiver->live && receiver->error.empty() && receiver->rows.size() == 1; }), "stop and fresh snapshot cannot mix previous query");
  start("name==\"*.live\""); check(count(1), "live search restarts");
  token->store(++generation);
  BMessage filtered(kSearchStart); filtered.AddInt64("serial", generation);
  filtered.AddString("root", scope.c_str()); filtered.AddString("expression", "name==\"*.live\"");
  filtered.AddBool("predicate", true); filtered.AddBool("live", true);
  SearchFilters filters; filters.minimumSize = "30"; WriteSearchFilters(filtered, filters);
  worker->PostMessage(&filtered); check(count(0), "live BFS initial results apply file size filter");
  std::ofstream(scope + "/same.live", std::ios::app) << std::string(40, 'x');
  check(count(1), "file grows into filtered live results");
  write(scope + "/same.live"); check(count(0), "file shrinks out of filtered live results");
  start("name==\"*.live\""); check(count(1), "clearing filter restores native query results");
  rename(scope.c_str(), (scope + "-old").c_str()); mkdir(scope.c_str(), 0700); write(scope + "/replacement.live");
  check(receiver->Await([&] { return receiver->serial == generation && !receiver->live && !receiver->error.empty() && receiver->rows.empty(); }), "replaced root stops live query instead of binding to new folder");
  start("((invalid"); check(receiver->Await([&] { return receiver->serial == generation && !receiver->live && !receiver->error.empty(); }), "invalid live formula fails without subscription");
  token->store(++generation); if (worker->Lock()) worker->Quit(); if (receiver->Lock()) receiver->Quit();
  check(owned && fs_remove_index(st.st_dev, index.c_str()) == 0, "owned attribute index removed after query closure");
  unlink((scope + "/replacement.live").c_str()); rmdir(scope.c_str());
  unlink((scope + "-old/same.live").c_str()); unlink((scope + "-old/old.no").c_str()); rmdir((scope + "-old").c_str());
  unlink((outside + "/branch/nested.live").c_str()); rmdir((outside + "/branch").c_str()); rmdir(outside.c_str()); rmdir(base.c_str());
  std::cout << checks << " checks, " << failures << " failures\n"; return failures ? 1 : 0;
}
