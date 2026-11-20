// SPDX-License-Identifier: MIT
#pragma once

#include <SplitView.h>
#include <array>

class ImagePreview;
struct WorkspaceLayout;

namespace Listi {
class PanelTools;
constexpr uint32 kSplitChanged = 'swgt';

class WorkspaceSplit : public BSplitView {
public:
  WorkspaceSplit(PanelTools *first, PanelTools *second);

  void PlaceInspector(bool first, float weight);
  void SetPaneVisible(int pane, bool visible);
  void SetInspectorVisible(bool visible);
  bool InspectorVisible() const { return inspectorVisible; }
  void ApplyGeometry(const WorkspaceLayout &layout, bool quickView, int source);
  void RememberWeights(WorkspaceLayout &layout, bool quickView) const;

  void MouseDown(BPoint where) override;
  void MouseUp(BPoint where) override;

  PanelTools &Panel(int pane) const { return *panes[pane]; }
  ImagePreview &Inspector() const { return *inspector; }
  int PanelContaining(const BView *view) const;
  bool HasPanelPreview() const;
  bool CloseEditors();
  bool CloseVisibleInspectorEditor();
  void UpdatePreviews(int source);
  void UpdateInspector(const WorkspaceLayout &layout, bool quickView,
                       int source);
  ImagePreview *OpenPanelPreview(int pane);

private:
  int SplitIndex(int logicalIndex) const;

  PanelTools *panes[2];
  ImagePreview *inspector;
  const float splitterSize;
  bool inspectorOnLeft = false;
  bool paneVisible[2] = {true, true};
  bool inspectorVisible = true;
  std::array<float, 3> before{};
};
}
