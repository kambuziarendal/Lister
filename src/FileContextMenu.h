// SPDX-License-Identifier: MIT
#pragma once
#include <Point.h>
#include <SupportDefs.h>
struct ContextAction { uint32 what = 0; bool folder = false; };
ContextAction ShowFileContextMenu(BPoint screen, bool single, bool directory,
    bool local, bool dual, bool folderLocal);
ContextAction ShowFolderContextMenu(BPoint screen, bool local);
