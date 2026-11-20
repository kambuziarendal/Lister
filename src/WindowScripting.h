// SPDX-License-Identifier: MIT
#pragma once

#include "WorkspaceLayout.h"

class BMessage;
class Pane;
class OperationQueue;

namespace Listi {
void AddWindowScriptingSuite(BMessage &message);
bool IsWindowScriptProperty(const char *name);
bool HandleWindowScript(BMessage &message, Workspace workspace, Pane &source,
                        Pane &destination, const OperationQueue &queue);
}
