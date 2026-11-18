// SPDX-License-Identifier: MIT
#pragma once

#include <string>

class BWindow;

void OpenInspectorEditor(const std::string &path, BWindow *owner);
bool InspectorEditorsAllowQuit();
