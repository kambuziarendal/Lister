// SPDX-License-Identifier: MIT
#pragma once
#include <View.h>
class Pane;
class PaneModel;
class FileGrid : public BView {
public:
  using BView::BView;
  virtual bool CancelSelectionGesture() = 0;
  virtual void Reset() = 0;
  virtual void SetMode(int) = 0;
  virtual void Update() = 0;
  virtual float ContentHeight(float width) const = 0;
  virtual void SyncSelection() = 0;
};
FileGrid *CreateFileGrid(Pane *, PaneModel *);
