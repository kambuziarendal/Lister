// SPDX-License-Identifier: MIT
#include "PaneModel.h"
#include <algorithm>
#include <iostream>
#include <set>
int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *s) {
    ++checks;
    failures += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << s << std::endl;
  };
  PaneModel model;
  auto add = [&](const std::string &name, bool directory = false,
                 int kind = 0) {
    auto e = std::make_unique<PaneEntry>();
    e->name = name;
    e->path = "/fixture/" + name;
    e->directory = directory;
    e->kind = kind;
    model.Add(std::move(e));
  };
  add("one.pdf");
  add("two.PDF");
  add(".hidden.pdf");
  add("image.jpg");
  add("music.mp3");
  add("folder.pdf", true, 1);
  add("link.pdf", false, 2);
  add("README");
  add(".profile");
  add("trailing.");
  add("a.tar.gz");
  add("b.TAR.GZ");
  add("single.gz");
  add("a.tar.lz4");
  add("single.lz4");
  add("a.tar.Z");
  add("single.Z");
  model.Filter("");
  model.SetSelection({"/fixture/image.jpg", "/fixture/one.pdf"},
                     "/fixture/one.pdf");
  check(model.FocusedFileType() == ".pdf",
        "clicked file determines type even in mixed selection");
  check(model.SelectSameFileType() && model.Selected().size() == 3,
        "same-type selection replaces selection with matching visible files");
  auto selected = model.Selected();
  check(std::find(selected.begin(), selected.end(), "/fixture/two.PDF") !=
            selected.end(),
        "filename extension comparison is case-insensitive");
  check(std::find(selected.begin(), selected.end(), "/fixture/.hidden.pdf") !=
            selected.end(),
        "visible dotfiles with ordinary extension match");
  check(std::find(selected.begin(), selected.end(), "/fixture/folder.pdf") ==
                selected.end() &&
            std::find(selected.begin(), selected.end(), "/fixture/link.pdf") ==
                selected.end(),
        "folders and links are never selected by file type");
  check(model.Focused()->path == "/fixture/one.pdf",
        "selection preserves clicked focus");
  model.Filter("one");
  check(model.SelectSameFileType() &&
            model.Selected() == std::vector<std::string>{"/fixture/one.pdf"},
        "active filter limits same-type selection");
  model.Filter("");
  model.SetSelection({"/fixture/a.tar.gz"}, "/fixture/a.tar.gz");
  check(model.SelectSameFileType() && model.Selected().size() == 2,
        "compound TAR extension is distinct from a plain gzip file");
  for (const auto *name : {"a.tar.lz4", "a.tar.Z"}) {
    const std::string path = "/fixture/" + std::string(name);
    model.SetSelection({path}, path);
    check(model.SelectSameFileType() && model.Selected() == std::vector<std::string>{path},
          "compressed TAR is distinct from a single compressed stream");
  }
  for (auto name :
       {"README", ".profile", "trailing.", "folder.pdf", "link.pdf"}) {
    auto path = "/fixture/" + std::string(name);
    model.SetSelection({path}, path);
    auto before = model.Selected();
    check(!model.SelectSameFileType() && model.Selected() == before,
          "untyped file, folder or link leaves selection unchanged");
  }
  model.Clear();
  check(!model.SelectSameFileType(), "empty pane refuses same-type selection");
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
