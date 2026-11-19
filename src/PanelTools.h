// SPDX-License-Identifier: MIT
#pragma once

#include <Button.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <array>

namespace Listi {
enum class PanelPage : int32 { Files, Search, Preview };
inline constexpr uint32 kPanelPage = 'ptpg', kClosePanelPage = 'ptcl';

// Keep the Pane attached even when a tool is visible. Detaching would
// invalidate its messenger and interrupt directory scans and selection.
class PanelTools : public BGroupView {
public:
  PanelTools(int32 pane, BView *files) : BGroupView(B_VERTICAL, 0) {
    SetName(pane == 0 ? "first panel tools" : "second panel tools");
    pages[0] = files;
    tabs = new BGroupView(B_HORIZONTAL, 2);
    tabs->SetName("panel tabs");
    const char *labels[] = {"Files", "Search", "Preview"};
    for (int32 i = 0; i < 3; ++i) {
      auto *choose = new BMessage(kPanelPage);
      choose->AddInt32("pane", pane);
      choose->AddInt32("page", i);
      buttons[i] = new BButton(labels[i], choose);
      buttons[i]->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);
      tabs->AddChild(buttons[i]);
      if (i != 0)
        buttons[i]->Hide();
    }
    auto *close = new BMessage(kClosePanelPage);
    close->AddInt32("pane", pane);
    closeButton = new BButton("×", close);
    closeButton->SetToolTip("Close this tool and return to files");
    BLayoutBuilder::Group<>(tabs).AddGlue().Add(closeButton);
    GroupLayout()->AddView(tabs, 0.0f);
    AddChild(files);
    tabs->Hide();
    buttons[0]->SetValue(B_CONTROL_ON);
  }

  PanelPage Current() const { return current; }
  BView *Page(PanelPage page) const { return pages[size_t(page)]; }
  void Open(PanelPage page, BView *view) {
    const auto index = size_t(page);
    pages[index] = view;
    AddChild(view);
    view->Hide();
    buttons[index]->Show();
    if (tabs->IsHidden(tabs))
      tabs->Show();
    Select(page);
  }

  void Select(PanelPage page) {
    if (!Page(page))
      return;
    if (current != page) {
      Page(current)->Hide();
      Page(page)->Show();
      current = page;
    }
    for (size_t i = 0; i < buttons.size(); ++i)
      buttons[i]->SetValue(i == size_t(current) ? B_CONTROL_ON : B_CONTROL_OFF);
    closeButton->SetEnabled(current != PanelPage::Files);
  }

  void Close(PanelPage page) {
    if (page == PanelPage::Files || !Page(page))
      return;
    if (current == page)
      Select(PanelPage::Files);
    auto index = size_t(page);
    GroupLayout()->RemoveView(pages[index]);
    delete pages[index];
    pages[index] = nullptr;
    buttons[index]->Hide();
    if (!pages[1] && !pages[2])
      tabs->Hide();
  }

  void EnableTools(bool enabled) {
    if (!enabled)
      Select(PanelPage::Files);
    bool show = enabled && (pages[1] || pages[2]);
    if (show == tabs->IsHidden(tabs)) {
      if (show)
        tabs->Show();
      else
        tabs->Hide();
    }
  }

private:
  PanelPage current = PanelPage::Files;
  BGroupView *tabs;
  BButton *closeButton;
  std::array<BView *, 3> pages{};
  std::array<BButton *, 3> buttons{};
};
} // namespace Listi
