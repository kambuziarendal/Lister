// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <Messenger.h>
constexpr uint32 kShowAttributeColumn = 'atcl', kClearAttributeColumns = 'atcc';
void OpenFileProperties(const std::string &path, BMessenger pane = {});
// Preflight every property draft before closing any application window.
bool FilePropertiesAllowQuit();
