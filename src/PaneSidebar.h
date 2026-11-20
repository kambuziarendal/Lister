// SPDX-License-Identifier: MIT
#pragma once

#include "SavedPlaces.h"
#include "Shelf.h"
#include "WorkspaceLayout.h"
#include <CardView.h>
#include <memory>

class BListView;
class BMenu;
class BMenuItem;
class BScrollView;
class FolderTree;
class Pane;
namespace Listi {
class NavigationHistory;
}

constexpr uint32 kToggleTree = 'ttre', kSidebarMode = 'psbr';

// The pane owns navigation and workspace preferences. This view owns the
// sidebar cards and the matching Places and View menu presentation.
class PaneSidebar : public BCardView {
public:
  PaneSidebar(Pane &, int32 paneId, BMenu &goMenu, BMenu &viewMenu,
              const Listi::NavigationHistory &, std::shared_ptr<Listi::Shelf>);

  void AttachedToWindow() override;
  void UpdateMode(const WorkspaceLayout &);
  void ToggleFocus();
  void ResetTree();
  void ShowTreePath();
  void SetPlaces(const Listi::SavedPlaces &, bool writable);
  void UpdateLocation();
  void RefreshShelf();
  bool ContextMenu(BView *target, BPoint point);
  bool HandleCommand(const BMessage &);

private:
  Pane &pane;
  int32 paneId;
  BMenu &viewMenu;
  const Listi::NavigationHistory &history;
  std::shared_ptr<Listi::Shelf> shelf;
  Listi::ShelfSnapshot renderedShelf;
  FolderTree *tree;
  BListView *favorites, *recentFolders, *shelfList;
  BScrollView *favoriteScroll;
  BMenu *placesMenu;
  Listi::SavedPlaces places;
  bool placesWritable = true;
  BMenuItem *sidebarMenuItem = nullptr;
  bool sidebarMenuConfigurable = false;
  SidebarMode visibleSidebar = SidebarMode::FolderTree;

  bool ContainsFocus() const;
  void RefreshRecentFolders();
  void UpdatePlaces();
  void OpenDestination(const char *path);
  void AddSidebarModes(BMenu &, bool menuField = false);
  void UpdateSidebarMenu(const WorkspaceLayout &);
};
