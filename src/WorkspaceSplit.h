// SPDX-License-Identifier: MIT
#pragma once

#include <SplitView.h>
#include <array>

struct WorkspaceLayout;

namespace Listi {
constexpr uint32 kSplitChanged = 'swgt';

class WorkspaceSplit : public BSplitView {
public:
  WorkspaceSplit(BView *first, BView *second, BView *inspector);

  void PlaceInspector(bool first, float weight);
  void SetPaneVisible(int pane, bool visible);
  void SetInspectorVisible(bool visible);
  bool InspectorVisible() const { return inspectorVisible; }
  void ApplyGeometry(const WorkspaceLayout &layout, bool quickView, int source);
  void RememberWeights(WorkspaceLayout &layout, bool quickView) const;

  void MouseDown(BPoint where) override;
  void MouseUp(BPoint where) override;

private:
  int SplitIndex(int logicalIndex) const;

  BView *panes[2];
  BView *inspector;
  const float splitterSize;
  bool inspectorOnLeft = false;
  bool paneVisible[2] = {true, true};
  bool inspectorVisible = true;
  std::array<float, 3> before{};
};
}
