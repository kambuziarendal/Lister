// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
#include <memory>
#include <string>

namespace Listi {
class Shelf;
constexpr uint32 kShowShelf = 'sfsh', kAddToShelf = 'sfad',
                 kRefreshShelf = 'sfrf', kSubmitShelfTransfer = 'sfst';
void OpenShelfWindow(BMessenger owner, const std::shared_ptr<Shelf> &,
                     BMessenger &window, const std::string &settingsFile = {});
} // namespace Listi
