// SPDX-License-Identifier: MIT
#include "PanelTools.h"
#include "ViewVisibility.h"

#include "ImagePreview.h"
#include "Pane.h"
#include "SearchWindow.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <Window.h>

namespace Listi {
PanelTools::PanelTools(int32 index, Pane &list)
    : BGroupView(B_VERTICAL, 0), pane(index), files(list) {
  SetName(pane == 0 ? "first panel tools" : "second panel tools");
  pages[0] = &files;
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
  AddChild(&files);
  tabs->Hide();
  UpdateTabs();
}

bool PanelTools::Select(PanelPage page) {
  if (!Page(page) || (current != page && !CanLeaveCurrent())) {
    // A toggle button can change itself before its message reaches us.
    UpdateTabs();
    return false;
  }
  ShowPage(page);
  return true;
}

bool PanelTools::CloseCurrent() {
  const auto page = current;
  if (page == PanelPage::Files)
    return true;
  if (!Select(PanelPage::Files))
    return false;
  const auto index = size_t(page);
  GroupLayout()->RemoveView(pages[index]);
  delete pages[index];
  pages[index] = nullptr;
  buttons[index]->Hide();
  if (!Page(PanelPage::Search) && !Page(PanelPage::Preview))
    tabs->Hide();
  return true;
}

bool PanelTools::CanLeaveCurrent() {
  return current != PanelPage::Preview || CloseEditor();
}

bool PanelTools::CloseEditor() {
  auto *preview = static_cast<ImagePreview *>(Page(PanelPage::Preview));
  return !preview || preview->CloseEditor();
}

void PanelTools::EnableTools(bool enabled) {
  if (!enabled)
    ShowPage(PanelPage::Files);
  const bool show =
      enabled && (Page(PanelPage::Search) || Page(PanelPage::Preview));
  SetViewVisible(tabs, show);
}

void PanelTools::FocusCurrent() {
  switch (current) {
  case PanelPage::Files:
    files.FocusList();
    break;
  case PanelPage::Search: {
    BMessage focus(kSearch);
    Page(current)->MessageReceived(&focus);
    break;
  }
  case PanelPage::Preview:
    Page(current)->MakeFocus();
    break;
  }
}

bool PanelTools::OpenSearch(const BMessage &store) {
  if (Page(PanelPage::Search))
    return Select(PanelPage::Search);
  if (!CanLeaveCurrent())
    return false;
  Open(PanelPage::Search,
       CreatePanelSearch(BMessenger(Window()), pane, files.Path(), store));
  return true;
}

BMessenger PanelTools::SearchMessenger() const {
  auto *search = Page(PanelPage::Search);
  return search ? BMessenger(search) : BMessenger();
}

ImagePreview &PanelTools::OpenPreview() {
  if (auto *preview = static_cast<ImagePreview *>(Page(PanelPage::Preview))) {
    ShowPage(PanelPage::Preview);
    return *preview;
  }
  auto *preview = new ImagePreview();
  Open(PanelPage::Preview, preview);
  return *preview;
}

void PanelTools::UpdatePreview(const PanelTools &opposite) {
  if (auto *preview = static_cast<ImagePreview *>(Page(PanelPage::Preview)))
    preview->ShowFile(current == PanelPage::Preview ? opposite.SelectedFile()
                                                    : "");
}

std::string PanelTools::SelectedFile() const {
  return current == PanelPage::Files ? files.SelectedPath() : "";
}

void PanelTools::Open(PanelPage page, BView *view) {
  const auto index = size_t(page);
  pages[index] = view;
  AddChild(view);
  view->Hide();
  buttons[index]->Show();
  SetViewVisible(tabs, true);
  ShowPage(page);
}

void PanelTools::ShowPage(PanelPage page) {
  if (current != page) {
    Page(current)->Hide();
    Page(page)->Show();
    current = page;
  }
  UpdateTabs();
}

void PanelTools::UpdateTabs() {
  for (size_t i = 0; i < buttons.size(); ++i)
    buttons[i]->SetValue(i == size_t(current) ? B_CONTROL_ON : B_CONTROL_OFF);
  closeButton->SetEnabled(current != PanelPage::Files);
}
} // namespace Listi
