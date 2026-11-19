// SPDX-License-Identifier: MIT
#pragma once
#include <MessageFilter.h>

namespace Listi {
class WindowKeyFilter : public BMessageFilter {
public:
  WindowKeyFilter();
  filter_result Filter(BMessage *message, BHandler **target) override;
};
} // namespace Listi
