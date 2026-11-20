// SPDX-License-Identifier: MIT
#pragma once

#include <GroupView.h>
#include <Messenger.h>
#include <array>
#include <string>

class BButton;
class ImagePreview;
class Pane;

namespace Listi {
enum class PanelPage : int32 { Files, Search, Preview };
inline constexpr uint32 kPanelPage = 'ptpg', kClosePanelPage = 'ptcl';

// Keep the Pane and any open tools attached when switching pages. Detaching
// would invalidate their messengers and interrupt scans and searches.
class PanelTools : public BGroupView {
public:
  PanelTools(int32 pane, Pane &files);

  Pane &Files() const { return files; }
  PanelPage Current() const { return current; }
  BView *Page(PanelPage page) const { return pages[size_t(page)]; }
  bool Select(PanelPage page);
  bool CloseCurrent();
  bool CloseEditor();
  void EnableTools(bool enabled);
  void FocusCurrent();
  bool OpenSearch(const BMessage &store);
  BMessenger SearchMessenger() const;
  ImagePreview &OpenPreview();
  void UpdatePreview(const PanelTools &opposite);
  std::string SelectedFile() const;

private:
  bool CanLeaveCurrent();
  void Open(PanelPage page, BView *view);
  void ShowPage(PanelPage page);
  void UpdateTabs();

  const int32 pane;
  Pane &files;
  PanelPage current = PanelPage::Files;
  BGroupView *tabs;
  BButton *closeButton;
  std::array<BView *, 3> pages{};
  std::array<BButton *, 3> buttons{};
};
} // namespace Listi
