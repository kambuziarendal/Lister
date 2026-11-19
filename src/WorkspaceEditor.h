// SPDX-License-Identifier: MIT
#pragma once

#include <Messenger.h>

struct Preferences;

inline constexpr uint32 kOpenWorkspaceEditor = 'ewsp';
inline constexpr uint32 kApplyWorkspaceEditor = 'waed';

void OpenWorkspaceEditor(BMessenger target, const Preferences &prefs,
                         bool showModules = false);
