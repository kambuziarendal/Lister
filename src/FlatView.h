// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
#include <memory>

class BView;

namespace Listi {
class Shelf;
constexpr uint32 kFlatView = 'flvw';
BView *CreateFlatView(BMessenger owner, std::shared_ptr<Shelf> shelf,
                        const char *root);
}
