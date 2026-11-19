// SPDX-License-Identifier: MIT
#include "Common.h"
#include <Application.h>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
  BApplication app("application/x-vnd.Kambuzi-Listi-WorkspaceLayoutTest");
  int failures = 0;
  auto check = [&](bool condition, const char *label) {
    failures += !condition;
    std::cout << (condition ? "PASS " : "FAIL ") << label << std::endl;
  };
  auto rejects = [](auto action) {
    try {
      action();
    } catch (const std::runtime_error &) {
      return true;
    }
    return false;
  };

  Preferences live;
  auto stale = live.Save();
  BMessage noChanges;
  stale.AddMessage("layoutChanges", &noChanges);
  live.workspace = Workspace::smidja;
  live.singlePane = 1;
  live.SetDefaultMode(Workspace::smidja, 2);
  live.SetPaneMode(0, 3);
  live.SetSidebar(0, SidebarMode::Hidden);
  live.SetSidebar(1, SidebarMode::Favorites);
  live.SetPreview(false);
  live.command = true;
  live.buttons = false;
  live.bankTop = true;
  live.commandTop = true;
  live.inspectorLeft = true;
  live.panelsStacked = true;
  live.treeRight = true;
  live.filterBottom = true;
  live.adaptive = true;
  live.actionButtons = {7, 2, 10};
  live.splitWeights[0] = 0.5f;
  live.splitWeights[1] = 0.25f;
  live.splitWeights[2] = 0.25f;
  live.folderModes["/remembered"] = MillerMode;
  live.profiles.Add("  Work bench  ", live.CaptureLayout());

  live.ApplyDialog(stale);
  check(live.DefaultMode(Workspace::smidja) == 2 && live.PaneMode(0) == 3 &&
            live.command && !live.buttons && live.adaptive &&
            live.profiles.Find("Work bench"),
        "unchanged stale Settings preserves newer live layout and profiles");
  BMessage commandEdit;
  commandEdit.AddBool("command", true);
  stale.ReplaceMessage("layoutChanges", &commandEdit);
  live.ApplyDialog(stale);
  check(!live.command && !live.buttons,
        "Settings only applies layout controls explicitly changed there");

  live.icons = false;
  live.editor = "/custom/editor";
  Preferences restored;
  restored.Load(live.Save());
  const auto *profile = restored.profiles.Find("Work bench");
  check(profile && profile->layout.workspace == Workspace::smidja &&
            profile->layout.singlePane == 1 && profile->layout.panelsStacked &&
            profile->layout.DefaultMode(Workspace::smidja) == 2 &&
            profile->layout.PaneMode(0) == 3 &&
            profile->layout.Sidebar(0) == SidebarMode::Hidden &&
            profile->layout.Sidebar(1) == SidebarMode::Favorites &&
            !profile->layout.Preview() && profile->layout.command &&
            !profile->layout.buttons,
        "stored profile retains the original workspace snapshot");
  check(restored.bankTop && restored.commandTop && restored.inspectorLeft &&
            restored.panelsStacked && restored.treeRight &&
            restored.filterBottom && restored.adaptive &&
            restored.actionButtons == std::vector<int32>({7, 2, 10}) &&
            restored.splitWeights[0] == 0.5f,
        "personal layout options and action order survive reload");
  restored.workspace = Workspace::tviskipt;
  check(restored.Sidebar(0) == SidebarMode::Hidden &&
            restored.Sidebar(1) == SidebarMode::Hidden,
        "Smiðja sidebars do not affect Tvískipt");
  restored.workspace = Workspace::smidja;
  check(restored.Sidebar(0) == SidebarMode::Hidden &&
            restored.Sidebar(1) == SidebarMode::Favorites,
        "Smiðja retains distinct sidebar choices after switching workspace");
  Preferences sidebars;
  sidebars.workspace = Workspace::askr;
  check(sidebars.Sidebar(0) == SidebarMode::FolderTree,
        "Askr keeps the original file tree default");
  sidebars.SetSidebar(0, SidebarMode::ShortcutsAndTree);
  sidebars.SetSidebar(0, SidebarMode::Hidden);
  sidebars.SetSidebar(1, SidebarMode::Shelf);
  sidebars.workspace = Workspace::smidja;
  sidebars.SetSidebar(0, SidebarMode::Shelf);
  sidebars.SetSidebar(0, SidebarMode::Hidden);
  sidebars.SetSidebar(1, SidebarMode::Favorites);
  Preferences sidebarReload;
  sidebarReload.Load(sidebars.Save());
  check(sidebarReload.Sidebar(0) == SidebarMode::Hidden &&
            sidebarReload.RestoreSidebar(0) == SidebarMode::Shelf &&
            sidebarReload.Sidebar(1) == SidebarMode::Favorites,
        "Smiðja persists each sidebar and remembers the mode of a hidden sidebar");
  sidebarReload.workspace = Workspace::askr;
  check(sidebarReload.Sidebar(0) == SidebarMode::Hidden &&
            sidebarReload.RestoreSidebar(0) == SidebarMode::ShortcutsAndTree &&
            sidebarReload.Sidebar(1) == SidebarMode::Shelf,
        "Askr persists its own independent sidebar choices and restore mode");
  sidebarReload.SetSidebar(0, SidebarMode::RecentFolders);
  sidebarReload.SetSidebar(0, SidebarMode::Hidden);
  sidebarReload.SetSidebar(Workspace::smidja, 1, SidebarMode::RecentFolders);
  sidebarReload.profiles.Add("Recent", sidebarReload.CaptureLayout());
  Preferences recentReload;
  recentReload.Load(sidebarReload.Save());
  const auto *recentProfile = recentReload.profiles.Find("Recent");
  check(recentReload.Sidebar(0) == SidebarMode::Hidden &&
            recentReload.RestoreSidebar(0) == SidebarMode::RecentFolders &&
            recentReload.Sidebar(1) == SidebarMode::Shelf && recentProfile &&
            recentProfile->layout.RestoreSidebar(0) == SidebarMode::RecentFolders,
        "Recent folders is restored when a hidden Askr sidebar is reopened");
  recentReload.workspace = Workspace::smidja;
  check(recentReload.Sidebar(1) == SidebarMode::RecentFolders &&
            recentReload.RestoreSidebar(0) == SidebarMode::Shelf,
        "Smiðja keeps Recent folders independent of the other pane and Askr");
  restored.ResetLayout();
  check(restored.workspace == Workspace::tviskipt && restored.buttons &&
            !restored.command && !restored.bankTop && !restored.commandTop &&
            !restored.inspectorLeft && !restored.panelsStacked &&
            !restored.treeRight && !restored.filterBottom &&
            !restored.adaptive &&
            restored.actionButtons == WorkspaceLayout{}.actionButtons,
        "reset restores only factory layout defaults");
  check(
      restored.profiles.Find("Work bench") == profile && !restored.icons &&
          restored.editor == "/custom/editor" &&
          restored.folderModes.at("/remembered") == MillerMode,
      "reset leaves profiles, appearance, launchers and folder choices intact");
  if (profile)
    restored.ApplyLayout(profile->layout);
  check(restored.workspace == Workspace::smidja && restored.command &&
            restored.actionButtons == std::vector<int32>({7, 2, 10}) &&
            !restored.icons && restored.editor == "/custom/editor",
        "applying a profile changes layout only");

  restored.profiles.Add("Temporary", WorkspaceLayout{});
  check(rejects(
            [&] { restored.profiles.Add("Work bench ", WorkspaceLayout{}); }) &&
            rejects(
                [&] { restored.profiles.Rename("Temporary", "Work bench"); }) &&
            rejects(
                [&] { restored.profiles.Rename("Temporary", "bad\x01name"); }),
        "duplicate or malformed names are rejected without overwriting");
  restored.profiles.Rename("Temporary", "Empty");
  restored.actionButtons.clear();
  restored.profiles.Update("Empty", restored.CaptureLayout());
  auto saved = restored.Save();
  restored.Load(saved);
  profile = restored.profiles.Find("Empty");
  check(!restored.profiles.Find("Temporary") && profile &&
            profile->layout.actionButtons.empty() &&
            restored.actionButtons.empty(),
        "renamed and updated profile persists an explicitly empty action bank");
  restored.profiles.Remove("Empty");
  saved = restored.Save();
  restored.Load(saved);
  check(!restored.profiles.Find("Empty") &&
            restored.profiles.Find("Work bench"),
        "deletion persists without affecting another profile");

  BMessage legacy;
  legacy.AddBool("simpleWorkspace", true);
  restored.Load(legacy);
  check(restored.IsEinn() && restored.profiles.Items().empty() &&
            !restored.adaptive && !restored.panelsStacked &&
            restored.actionButtons.front() == 0 &&
            restored.actionButtons.back() == 11,
        "legacy settings keep layout defaults and start without profiles");

  BMessage malformed, bank, records, invalid, valid, state;
  records.AddInt32("version", 1);
  invalid.AddString("name", "Broken");
  records.AddMessage("profile", &invalid);
  WorkspaceLayout{}.Archive(state);
  valid.AddString("name", "Readable");
  valid.AddMessage("layout", &state);
  records.AddMessage("profile", &valid);
  records.AddMessage("profile", &valid);
  malformed.AddMessage("workspaceProfiles", &records);
  bank.AddInt32("count", 2);
  bank.AddInt32("action", 8);
  bank.AddInt32("action", 8);
  malformed.AddMessage("actionBank", &bank);
  malformed.AddFloat("splitWeight", std::numeric_limits<float>::infinity());
  malformed.AddFloat("splitWeight", 0.5f);
  malformed.AddFloat("splitWeight", 0.5f);
  restored.Load(malformed);
  check(restored.actionButtons == WorkspaceLayout{}.actionButtons &&
            restored.splitWeights[0] == 0.39f,
        "corrupt action bank and pane weights safely fall back to defaults");
  check(restored.profiles.Items().size() == 1 &&
            restored.profiles.Find("Readable"),
        "malformed and duplicate profiles do not remove a valid sibling");

  return failures ? 1 : 0;
}
