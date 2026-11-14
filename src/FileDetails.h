// SPDX-License-Identifier: MIT
#pragma once
#include <View.h>
class Pane;
class PaneModel;
struct Preferences;
class FileDetails : public BView {
public:
  using BView::BView;
  virtual void Apply(const Preferences &) = 0;
  virtual void Refresh() = 0;
  virtual void SyncSelection() = 0;
  virtual void Redraw() = 0;
};
FileDetails *CreateFileDetails(Pane *, PaneModel *, const Preferences &);
