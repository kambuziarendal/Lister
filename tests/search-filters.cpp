// SPDX-License-Identifier: MIT
#include "FileSearch.h"
#include "SavedSearches.h"
#include <Application.h>
#include <Node.h>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <utime.h>
using namespace Listi;
int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-SearchFiltersTest");
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) { ++checks; failures += !ok; std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl; };
  auto rejects = [&](auto fn, const char *label) { bool caught = false; try { fn(); } catch (const std::exception &) { caught = true; } check(caught, label); };
  SearchFilters f;
  struct stat st{}; st.st_mode = S_IFREG | 0600; st.st_size = 1024;
  f.extension = ".PDF"; auto c = CompileSearchFilters(f);
  check(c.Matches("/x/a.pdf", st) && c.Matches("/x/b.PDF", st) && !c.Matches("/x/.pdf", st) && !c.Matches("/x/a.pdf.old", st), "extension case folding and dotfile/suffix boundaries");
  f.extension = "tar.gz"; check(CompileSearchFilters(f).Matches("/a/b.TAR.GZ", st), "compound extension");
  f.extension = "Å"; check(CompileSearchFilters(f).Matches("/a/b.å", st), "UTF-8 extension folding");
  for (auto bad : {".", "*.pdf", "p/df", "pdf txt"}) { f.extension = bad; rejects([&] { CompileSearchFilters(f); }, "invalid extension refused"); }
  f = {}; f.minimumSize = "1 KiB"; f.maximumSize = "1024 B"; c = CompileSearchFilters(f);
  check(c.Matches("a", st), "inclusive equivalent byte and KiB bounds");
  st.st_size = 1023; check(!c.Matches("a", st), "below minimum excluded"); st.st_size = 1025; check(!c.Matches("a", st), "above maximum excluded");
  f = {}; f.maximumSize = "0"; st.st_size = 0; check(CompileSearchFilters(f).Matches("a", st), "zero-byte files can be selected explicitly");
  for (auto size : {"-1", "1.5 MiB", "1 MB", "1GiBjunk", "9223372036854775808", "9223372036854775807 GiB"}) {
    f.minimumSize = size; rejects([&] { CompileSearchFilters(f); }, "invalid or overflowing size refused");
  }
  f.minimumSize = "2"; f.maximumSize = "1"; rejects([&] { CompileSearchFilters(f); }, "reversed size interval refused");
  f = {}; f.minimumSize = "2 MiB"; f.maximumSize = "1 GiB"; c = CompileSearchFilters(f);
  check(c.minimum == 2097152 && c.maximum == 1073741824, "binary size units resolved without floating point");
  f = {}; f.fromDate = "2024-02-29"; f.throughDate = "2024-02-29"; c = CompileSearchFilters(f);
  st.st_mtime = c.from; check(c.Matches("a", st), "from calendar day included");
  st.st_mtime = c.until - 1; check(c.Matches("a", st), "last second of through calendar day included");
  st.st_mtime = c.until; check(!c.Matches("a", st), "following midnight excluded");
  st.st_mtime = c.from - 1; check(!c.Matches("a", st), "second before from excluded");
  st.st_crtime = c.from; f.created = true; check(CompileSearchFilters(f).Matches("a", st), "creation uses Haiku birth time, not modification or ctime");
  for (auto date : {"2023-02-29", "2024-13-01", "2024-04-31", "24-01-01", "2024-01-00", "1969-01-01"}) {
    f.fromDate = date; rejects([&] { CompileSearchFilters(f); }, "invalid calendar date refused");
  }
  f.fromDate = "2024-03-01"; rejects([&] { CompileSearchFilters(f); }, "reversed date interval refused");
  f = {}; f.fromDate = "1970-01-01"; check(CompileSearchFilters(f).from != INT64_MIN, "epoch-day local timezone accepted");
  // Haiku uses ICU timezone IDs; change only this process, not system settings.
  const char *old = getenv("TZ"); std::string savedTZ = old ? old : ""; bool hadTZ = old;
  setenv("TZ", "Europe/Oslo", 1); tzset();
  f = {}; f.fromDate = f.throughDate = "2024-03-31"; c = CompileSearchFilters(f);
  check(c.until - c.from == 23 * 3600, "spring daylight-saving day has 23 hours");
  f.fromDate = f.throughDate = "2024-10-27"; c = CompileSearchFilters(f);
  check(c.until - c.from == 25 * 3600, "autumn daylight-saving day has 25 hours");
  if (hadTZ) setenv("TZ", savedTZ.c_str(), 1); else unsetenv("TZ"); tzset();
  f = {}; f.extension = std::string("pdf\0x", 5); rejects([&] { CompileSearchFilters(f); }, "embedded NUL refused");
  f.extension = std::string(256, 'x'); rejects([&] { CompileSearchFilters(f); }, "oversized filter refused");
  f = {}; f.minimumSize = "0"; c = CompileSearchFilters(f);
  st.st_mode = S_IFDIR; check(!c.Matches("a", st), "active file filter excludes folders");
  st.st_mode = S_IFLNK; check(!c.Matches("a", st), "active file filter excludes symlinks");
  check(CompileSearchFilters({}).Matches("a", st), "no filters preserves existing symlink search");

  char pattern[] = "/boot/home/listi-filter-test-XXXXXX"; char *dir = mkdtemp(pattern); if (!dir) return 2;
  std::string root = dir; mkdir((root + "/nested.pdf").c_str(), 0700);
  std::ofstream(root + "/small.pdf") << "abc"; std::ofstream(root + "/nested.pdf/big.PDF") << "12345678";
  std::ofstream(root + "/other.txt") << "12345678";
  symlink((root + "/nested.pdf/big.PDF").c_str(), (root + "/linked.pdf").c_str());
  f = {}; f.extension = "pdf"; f.minimumSize = "8"; f.maximumSize = "8";
  f.fromDate = f.throughDate = "2024-02-29"; c = CompileSearchFilters(f);
  utimbuf times{time_t(c.from), time_t(c.until - 1)}; utime((root + "/nested.pdf/big.PDF").c_str(), &times);
  SearchRequest r; r.root = root; r.filters = f;
  std::vector<SearchHit> hits;
  auto run = [&] { hits.clear(); return SearchFiles(r, [] { return true; }, [&](const auto &h) { hits.push_back(h); return true; }); };
  auto summary = run();
  check(summary.error.empty() && hits.size() == 1 && hits[0].path == root + "/nested.pdf/big.PDF", "filter-only recursive search combines all conditions without blocking traversal");
  r.expression = "small"; check(run().matched == 0, "name condition AND file filters");
  r.predicate = true; r.expression = "(name==\"*.pdf\")||(name==\"*.PDF\")";
  summary = run(); check(summary.error.empty() && hits.size() == 1 && hits[0].path == root + "/nested.pdf/big.PDF", "native BFS results use same filters without following symlink targets");
  r.filters.minimumSize = "9"; check(!run().error.empty(), "invalid request fails rather than returning empty success");
  r.filters = f; summary = SearchFiles(r, [] { return false; }, [](const auto &) { return true; }); check(summary.cancelled, "filtered search respects pre-cancellation");

  SavedSearches store; SavedSearch item; item.name = "Filter only"; item.request.root = root; item.request.filters = f;
  store.Put(item, false); auto restored = SavedSearches::FromMessage(store.Message());
  const auto *copy = restored.Find(item.name);
  check(copy && copy->request.expression.empty() && copy->request.filters.throughDate == f.throughDate && copy->request.filters.maximumSize == "8", "filter-only saved search roundtrip preserves user criteria");
  auto message = store.Message(); BMessage entry; message.FindMessage("search", &entry);
  entry.RemoveName("filters"); message.ReplaceMessage("search", &entry);
  rejects([&] { SavedSearches::FromMessage(message); }, "v2 missing filters fails closed");
  entry.ReplaceString("expression", "legacy"); message.ReplaceInt32("version", 1); message.ReplaceMessage("search", &entry);
  restored = SavedSearches::FromMessage(message); copy = restored.Find(item.name);
  check(copy && copy->request.expression == "legacy" && !copy->request.filters.Active(), "v1 criteria migrate without invented filters");
  BMessage malformed; malformed.AddString("filters", "bad"); rejects([&] { ReadSearchFilters(malformed); }, "wrong typed filter block rejected");
  BMessage empty; WriteSearchFilters(empty, f); empty.AddMessage("filters", &entry); rejects([&] { ReadSearchFilters(empty); }, "duplicate filter block rejected");
  std::string file = root + "/criteria"; store.Save(file);
  check(SavedSearches::Load(file).Find(item.name)->request.filters.extension == "pdf", "filters survive actual atomic disk save/load");
  unlink(file.c_str()); unlink((root + "/small.pdf").c_str()); unlink((root + "/nested.pdf/big.PDF").c_str());
  unlink((root + "/other.txt").c_str()); unlink((root + "/linked.pdf").c_str()); rmdir((root + "/nested.pdf").c_str()); rmdir(root.c_str());
  std::cout << checks << " checks, " << failures << " failures\n"; return failures ? 1 : 0;
}
