// SPDX-License-Identifier: MIT
#pragma once
#include <Point.h>
#include "PaneModel.h"
#include <SupportDefs.h>
struct ContextAction { uint32 what = 0; bool folder = false; int value = 0; bool ascending = true; };
ContextAction ShowFileContextMenu(BPoint screen, bool single, bool directory,
    bool local, bool dual, bool folderLocal, PaneSort sort, int view);
ContextAction ShowFolderContextMenu(BPoint screen, bool local, PaneSort sort, int view);
