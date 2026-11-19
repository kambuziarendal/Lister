// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
BMessenger OpenFileSearch(BMessenger owner, int32 pane, const char *root,
                          const BMessage &store);
class BView;
// Keep this view attached while switching tabs so an active search keeps its
// worker and results. Removing and deleting it cancels and joins the worker.
BView *CreatePanelSearch(BMessenger owner, int32 pane, const char *root,
                         const BMessage &store);
