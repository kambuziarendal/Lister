// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
#include <memory>

namespace Listi {
class Shelf;
constexpr uint32 kFlatView = 'flvw';
BMessenger OpenFlatView(BMessenger owner, std::shared_ptr<Shelf> shelf,
                        const char *root);
}
