// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>

class BWindow;
struct Preferences;
namespace Listi {
class SavedPlaces;
class SavedSearches;
}

constexpr uint32 kQuickView = 'qvpr', kInlineNotice = 'inot';

BWindow *CreateMainWindow(const Preferences &, BMessenger service,
                          const Listi::SavedPlaces &, bool placesWritable,
                          const Listi::SavedSearches &, bool searchesWritable);
void PostMainWindowNotice(const char *text);
