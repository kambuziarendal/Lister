// SPDX-License-Identifier: MIT
#include "SavedSearches.h"
#include "MessageStore.h"
#include <Application.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Lister;
int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-SavedSearchTest");
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) { ++checks; failures += !ok; std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl; };
  auto rejects = [&](auto fn, const char *label) { bool caught = false; try { fn(); } catch (const std::exception &) { caught = true; } check(caught, label); };
  SavedSearches saved;
  SavedSearch item{"  Tekst Å  ", {"/boot/home/offline-synthetic", "name==\"*.txt\"", true, false}, true};
  saved.Put(item, false);
  check(saved.Find("Tekst Å") && saved.Items().size() == 1, "trimmed Unicode search name, offline root accepted without executing query");
  auto restored = SavedSearches::FromMessage(saved.Message());
  const auto *copy = restored.Find("Tekst Å");
  check(copy && copy->live && copy->request.predicate && !copy->request.includeTrash && copy->request.expression == item.request.expression, "all criteria roundtrip, no results stored");
  rejects([&] { saved.Put(item, false); }, "duplicate name rejected");
  item.request.expression = "name==\"*.cpp\""; item.request.includeTrash = true; saved.Put(item, true);
  check(saved.Find("Tekst Å")->request.includeTrash && saved.Find("Tekst Å")->request.expression == item.request.expression, "explicit update replaces criteria");
  auto invalid = item; invalid.name = "missing";
  rejects([&] { saved.Put(invalid, true); }, "missing update refused");
  invalid.name = "bad\nname"; rejects([&] { saved.Put(invalid, false); }, "control character name refused");
  invalid.name = std::string(81, 'x'); rejects([&] { saved.Put(invalid, false); }, "oversized name refused");
  invalid.name = ""; rejects([&] { saved.Put(invalid, false); }, "empty name refused");
  invalid = item; invalid.name = "bad"; invalid.request.root = "smb://host/share";
  rejects([&] { saved.Put(invalid, false); }, "unsupported provider refused");
  invalid.request.root = "/boot/home"; invalid.request.expression = "";
  rejects([&] { saved.Put(invalid, false); }, "empty expression refused");
  invalid.request.expression = std::string(4097, 'x'); rejects([&] { saved.Put(invalid, false); }, "oversized expression refused");
  invalid.request.expression = std::string("a\0b", 3); rejects([&] { saved.Put(invalid, false); }, "embedded NUL refused");
  invalid = item; invalid.name = "bad"; invalid.request.predicate = false;
  rejects([&] { saved.Put(invalid, false); }, "live literal mode refused");
  invalid.live = false; saved.Put(invalid, false);
  check(saved.Find("bad") && !saved.Find("bad")->request.predicate, "literal snapshot criteria supported");
  saved.Remove("bad"); check(!saved.Find("bad") && saved.Items().size() == 1, "forget removes definition only");
  rejects([&] { saved.Remove("missing"); }, "missing forget refused");
  BMessage wrong = saved.Message(); wrong.ReplaceInt32("version", 2);
  rejects([&] { SavedSearches::FromMessage(wrong); }, "unknown format preserved by refusal");
  BMessage malformed; malformed.AddInt32("version", 1); malformed.AddString("search", "wrong");
  rejects([&] { SavedSearches::FromMessage(malformed); }, "wrong list type refused");
  BMessage entry; entry.AddString("name", "broken"); malformed.RemoveName("search"); malformed.AddMessage("search", &entry);
  rejects([&] { SavedSearches::FromMessage(malformed); }, "missing typed criteria refused");
  SavedSearches full;
  for (size_t i = 0; i < SavedSearches::Limit; ++i) { item.name = "Search " + std::to_string(i); full.Put(item, false); }
  check(SavedSearches::FromMessage(full.Message()).Items().size() == 64, "64-search boundary roundtrips");
  item.name = "overflow"; rejects([&] { full.Put(item, false); }, "65th definition refused");
  std::string file = "/boot/home/listi-saved-search-test-" + std::to_string(getpid());
  check(SavedSearches::Load(file).Items().empty(), "missing store starts empty");
  saved.Save(file); check(SavedSearches::Load(file).Find("Tekst Å"), "native atomic disk roundtrip");
  auto read = [&] { std::ifstream f(file); return std::string(std::istreambuf_iterator<char>(f), {}); };
  auto before = read(); rejects([&] { saved.Save(file + "/missing"); }, "write error reported");
  check(read() == before, "write failure preserves previous bytes");
  SaveMessageStore(file, wrong); before = read(); rejects([&] { SavedSearches::Load(file); }, "unknown on-disk format refused");
  check(before == read(), "unknown on-disk format unchanged");
  std::ofstream(file, std::ios::trunc) << "corrupt"; rejects([&] { SavedSearches::Load(file); }, "corrupt store refused");
  check(read() == "corrupt", "corrupt store untouched");
  BMessage empty; SaveMessageStore(file, empty); rejects([&] { SavedSearches::Load(file); }, "empty serialized message is not a missing file");
  std::ofstream(file, std::ios::trunc); rejects([&] { SavedSearches::Load(file); }, "zero-byte store refused");
  unlink(file.c_str()); std::cout << checks << " checks, " << failures << " failures\n"; return failures ? 1 : 0;
}
