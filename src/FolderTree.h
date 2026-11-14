// SPDX-License-Identifier: MIT
#pragma once
#include <Messenger.h>
#include <View.h>
#include <atomic>
#include <map>
#include <memory>
#include <string>
class BLooper;
class TreeList;
class TreeItem;
class FolderTree : public BView {
public:
  explicit FolderTree();
  ~FolderTree();
  void AttachedToWindow() override;
  void MessageReceived(BMessage *) override;
  void ShowPath(const std::string &);
  void NavigateSelection(const std::string &);
  void ExpandPath(const std::string &);

private:
  TreeList *list;
  BLooper *reader = nullptr;
  std::shared_ptr<std::atomic<int64>> generation;
  std::map<std::string, TreeItem *> items;
  std::string root, desired;
  void Clear();
  void Follow();
};
