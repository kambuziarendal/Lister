// SPDX-License-Identifier: MIT
#include "WorkspaceSplit.h"

#include "ImagePreview.h"
#include "PanelTools.h"
#include "WorkspaceLayout.h"
#include <Layout.h>
#include <Window.h>

namespace Listi {
namespace {
constexpr float kWorkspaceSpacing = 6;
constexpr int kInspector = 2;
}

WorkspaceSplit::WorkspaceSplit(PanelTools *first, PanelTools *second)
    : BSplitView(B_HORIZONTAL, kWorkspaceSpacing), panes{first, second},
      inspector(new ImagePreview()), splitterSize(SplitterSize()) {
  AddChild(first);
  AddChild(second);
  AddChild(inspector);
  const WorkspaceLayout defaults;
  for (int i = 0; i < 3; ++i)
    SetItemWeight(i, defaults.splitWeights[i], i == kInspector);
  SetCollapsible(0, false);
  SetCollapsible(1, false);
}

int WorkspaceSplit::SplitIndex(int logicalIndex) const {
  if (inspectorOnLeft)
    return logicalIndex == kInspector ? 0 : logicalIndex + 1;
  return logicalIndex;
}

void WorkspaceSplit::PlaceInspector(bool first, float weight) {
  if (inspectorOnLeft == first)
    return;

  GetLayout()->RemoveView(inspector);
  inspectorOnLeft = first;
  AddChild(SplitIndex(kInspector), inspector, weight);
  for (int i = 0; i < 3; ++i)
    SetCollapsible(SplitIndex(i), i == kInspector);
}

void WorkspaceSplit::SetPaneVisible(int pane, bool visible) {
  if (paneVisible[pane] == visible)
    return;

  paneVisible[pane] = visible;
  if (visible)
    panes[pane]->Show();
  else
    panes[pane]->Hide();
}

void WorkspaceSplit::SetInspectorVisible(bool visible) {
  if (inspectorVisible == visible)
    return;

  inspectorVisible = visible;
  if (visible && inspector->IsHidden(inspector))
    inspector->Show();
  else if (!visible && !inspector->IsHidden(inspector))
    inspector->Hide();
}

void WorkspaceSplit::ApplyGeometry(const WorkspaceLayout &layout, bool quickView,
                                   int source) {
  SetOrientation(layout.panelsStacked ? B_VERTICAL : B_HORIZONTAL);
  const bool oneView = layout.SinglePane() && !quickView;
  SetSpacing(oneView ? 0 : kWorkspaceSpacing);
  SetSplitterSize(oneView ? 0 : splitterSize);
  for (int i = 0; i < 3; ++i)
    SetItemWeight(SplitIndex(i), layout.splitWeights[i], true);
  if (quickView)
    SetItemWeight(SplitIndex(kInspector), layout.splitWeights[1 - source], true);
}

void WorkspaceSplit::RememberWeights(WorkspaceLayout &layout,
                                     bool quickView) const {
  if (layout.SinglePane() || quickView)
    return;

  // Keep the inspector's previous share when it is not participating.
  const bool inspectorExpanded =
      inspectorVisible && !inspector->IsHidden(inspector);
  const int count = inspectorExpanded ? 3 : 2;
  const float totalShare =
      inspectorExpanded ? 1 : 1 - layout.splitWeights[kInspector];
  float raw[3]{}, total = 0;
  for (int i = 0; i < count; ++i) {
    raw[i] = ItemWeight(SplitIndex(i));
    total += raw[i];
  }
  if (total <= 0)
    return;
  for (int i = 0; i < count; ++i)
    layout.splitWeights[i] = raw[i] * totalShare / total;
}

void WorkspaceSplit::MouseDown(BPoint where) {
  for (int32 i = 0; i < CountItems(); ++i)
    before[i] = ItemWeight(i);
  BSplitView::MouseDown(where);
}

void WorkspaceSplit::MouseUp(BPoint where) {
  BSplitView::MouseUp(where);
  for (int32 i = 0; i < CountItems(); ++i) {
    if (before[i] != ItemWeight(i)) {
      Window()->PostMessage(kSplitChanged);
      break;
    }
  }
}

int WorkspaceSplit::PanelContaining(const BView *view) const {
  for (; view; view = view->Parent()) {
    if (view == panes[0])
      return 0;
    if (view == panes[1])
      return 1;
  }
  return -1;
}

bool WorkspaceSplit::HasPanelPreview() const {
  return Panel(0).Current() == PanelPage::Preview ||
         Panel(1).Current() == PanelPage::Preview;
}

bool WorkspaceSplit::CloseEditors() {
  return Inspector().CloseEditor() && Panel(0).CloseEditor() &&
         Panel(1).CloseEditor();
}

bool WorkspaceSplit::CloseVisibleInspectorEditor() {
  return !InspectorVisible() || Inspector().CloseEditor();
}

void WorkspaceSplit::UpdatePreviews(int source) {
  Panel(0).UpdatePreview(Panel(1));
  Panel(1).UpdatePreview(Panel(0));
  Inspector().ShowFile(InspectorVisible() ? Panel(source).SelectedFile() : "");
}

void WorkspaceSplit::UpdateInspector(const WorkspaceLayout &layout,
                                     bool quickView, int source) {
  Inspector().SetQuickViewCloseVisible(quickView);
  SetInspectorVisible(quickView || (layout.Preview() && !layout.SinglePane() &&
                                    !HasPanelPreview()));
  UpdatePreviews(source);
}

ImagePreview *WorkspaceSplit::OpenPanelPreview(int pane) {
  return CloseVisibleInspectorEditor() ? &Panel(pane).OpenPreview() : nullptr;
}
}
