// SPDX-License-Identifier: MIT
#include "SavedPlaces.h"
#include <Application.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Listi;
int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-PlacesTest");
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) {
    ++checks; failures += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
  };
  auto rejects = [&](auto work, const char *label) {
    bool caught = false;
    try { work(); } catch (const std::exception &) { caught = true; }
    check(caught, label);
  };
  SavedPlaces places;
  places.Add("  Work / Å  ", "/boot/home/work");
  check(places.Items()[0].name == "Work / Å", "label trims whitespace, permits slash and Unicode");
  rejects([&]{ places.Add("other", "/boot/home/work"); }, "duplicate address refused");
  rejects([&]{ places.Add("Work / Å", "/other"); }, "duplicate label refused");
  for (const char *label : {"", " \n", "bad\nname"})
    rejects([&]{ places.Add(label, "/other"); }, "invalid label refused");
  rejects([&]{ places.Add(std::string(81, 'x'), "/other"); }, "oversized label refused");
  for (const char *path : {"relative", "smb://user:secret@host/share", "sftp://user@host/path", "ftp://host:bad/path"})
    rejects([&]{ places.Add("invalid", path); }, "invalid or credential-bearing address refused");
  rejects([&]{ places.Add("null", std::string("/a\0b", 4)); }, "NUL address refused");
  places.Add("Offline server", "smb://offline.example/share");
  places.Add("Disks", "@disks");
  places.Add("Archive", "archive:///boot/home/sample.zip!/");
  auto restored = SavedPlaces::FromMessage(places.Message());
  check(restored.Items().size() == 4 && restored.Find("@disks"), "local, remote, disks and archive roundtrip without opening targets");
  places.Rename("/boot/home/work", "Renamed");
  check(places.Find("/boot/home/work")->name == "Renamed", "rename retains address");
  rejects([&]{ places.Rename("@disks", "Renamed"); }, "rename collision refused");
  rejects([&]{ places.Rename("/missing", "unused"); }, "rename missing entry refused");
  places.Remove("@disks");
  check(!places.Find("@disks") && places.Items().size() == 3, "forget removes only addressed entry");
  rejects([&]{ places.Remove("/missing"); }, "forget missing entry refused");
  BMessage wrong; wrong.AddInt32("version", 2);
  rejects([&]{ SavedPlaces::FromMessage(wrong); }, "unknown version refused");
  BMessage broken; broken.AddInt32("version", 1); broken.AddString("place", "wrong type");
  rejects([&]{ SavedPlaces::FromMessage(broken); }, "wrong entry type refused");
  SavedPlaces full;
  for (unsigned i = 0; i < SavedPlaces::Limit; ++i)
    full.Add("Place " + std::to_string(i), "/" + std::to_string(i));
  check(SavedPlaces::FromMessage(full.Message()).Items().size() == SavedPlaces::Limit, "64-place boundary roundtrips");
  rejects([&]{ full.Add("overflow", "/overflow"); }, "65th place refused");
  SavedPlaces labels;
  const auto longLeaf = std::string(79, 'x') + "å";
  const auto longLabel = labels.SuggestedName("/one/" + longLeaf);
  labels.Add(longLabel, "/one/" + longLeaf);
  check(longLabel == std::string(79, 'x'),
        "suggested name does not split a multibyte character at the byte limit");
  const auto secondLabel = labels.SuggestedName("/two/" + longLeaf);
  labels.Add(secondLabel, "/two/" + longLeaf);
  const auto thirdLabel = labels.SuggestedName("/three/" + longLeaf);
  labels.Add(thirdLabel, "/three/" + longLeaf);
  check(secondLabel == std::string(76, 'x') + " (2)" &&
            thirdLabel == std::string(76, 'x') + " (3)",
        "long duplicate names reserve room for a unique suffix");
  check(labels.SuggestedName("/line\nbreak") == "Place",
        "invalid folder label gets a valid fallback");
  std::string file = "/boot/home/listi-places-test-" + std::to_string(getpid());
  check(SavedPlaces::Load(file).Items().empty(), "missing store starts empty");
  places.Save(file);
  check(SavedPlaces::Load(file).Items()[0].name == "Renamed", "atomic native disk roundtrip");
  places.Rename("/boot/home/work", "Updated"); places.Save(file);
  check(SavedPlaces::Load(file).Items()[0].name == "Updated", "atomic update replaces previous store");
  rejects([&]{ places.Save(file + "/missing"); }, "failed save is reported");
  check(SavedPlaces::Load(file).Items()[0].name == "Updated", "failed save does not alter committed store");
  { std::ofstream out(file, std::ios::trunc); out << "corrupt-store"; }
  rejects([&]{ SavedPlaces::Load(file); }, "corrupt store refused");
  std::ifstream in(file); std::string content; in >> content;
  check(content == "corrupt-store", "failed load preserves corrupt original for recovery");
  unlink(file.c_str());
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
