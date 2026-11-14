// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
#include <Message.h>
class BView;
BView *CreateConnectionView(BMessenger pane, BMessenger closeTarget,
                            const BMessage &closeMessage);
