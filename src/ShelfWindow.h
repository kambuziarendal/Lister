// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
#include <memory>

namespace Listi {
class Shelf;
constexpr uint32 kShowShelf = 'sfsh', kAddToShelf = 'sfad',
                 kRefreshShelf = 'sfrf', kSubmitShelfMove = 'sfsm';
void OpenShelfWindow(BMessenger owner, const std::shared_ptr<Shelf> &,
                     BMessenger &window);
} // namespace Listi
