// SPDX-License-Identifier: MIT
#include "WindowScripting.h"

#include "OperationQueue.h"
#include "Pane.h"
#include <Message.h>
#include <PropertyInfo.h>
#include <cstring>

namespace Listi {
namespace {
// Keep the dispatch identifiers in the order of the published suite.
enum Property { WorkspaceValue, SourcePath, DestinationPath, PathReady, Busy, Unknown };

property_info kProperties[] = {
    {"Workspace", {B_GET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0},
     "Current workspace", 0},
    {"SourcePath", {B_GET_PROPERTY, B_SET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0},
     "Source directory", 0},
    {"DestinationPath", {B_GET_PROPERTY, B_SET_PROPERTY, 0},
     {B_DIRECT_SPECIFIER, 0}, "Destination directory", 0},
    {"PathReady", {B_GET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0},
     "Both directory scans complete", 0},
    {"Busy", {B_GET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0},
     "Operations are queued or running", 0},
    {nullptr}};
static_assert(sizeof(kProperties) / sizeof(kProperties[0]) == Unknown + 1);

Property FindProperty(const char *name) {
  for (int i = 0; i < Unknown; ++i) {
    if (std::strcmp(kProperties[i].name, name) == 0)
      return static_cast<Property>(i);
  }
  return Unknown;
}

status_t ReplyToProperty(const BMessage &message, Property property,
                         Workspace workspace, Pane &source, Pane &destination,
                         const OperationQueue &queue, BMessage &reply) {
  if (property == SourcePath || property == DestinationPath) {
    Pane &pane = property == SourcePath ? source : destination;
    if (message.what == B_SET_PROPERTY) {
      const char *value;
      if (message.FindString("data", &value) != B_OK)
        return B_BAD_VALUE;
      pane.Navigate(value);
    }
    reply.AddString("result", pane.Path());
    return B_OK;
  }

  if (message.what == B_SET_PROPERTY)
    return B_NOT_ALLOWED;

  switch (property) {
  case WorkspaceValue:
    reply.AddString("result", WorkspaceName(workspace));
    break;
  case PathReady:
    reply.AddBool("result", !source.IsScanning() && !destination.IsScanning());
    break;
  case Busy:
    reply.AddBool("result", queue.Busy());
    break;
  default:
    break;
  }
  return B_OK;
}
}

void AddWindowScriptingSuite(BMessage &message) {
  BPropertyInfo info(kProperties);
  message.AddString("suites", "suite/vnd.Kambuzi-Lister");
  message.AddFlat("messages", &info);
}

bool IsWindowScriptProperty(const char *name) {
  return FindProperty(name) != Unknown;
}

bool HandleWindowScript(BMessage &message, Workspace workspace, Pane &source,
                        Pane &destination, const OperationQueue &queue) {
  if (message.what != B_GET_PROPERTY && message.what != B_SET_PROPERTY)
    return false;

  int32 index, what;
  BMessage specifier;
  const char *name;
  if (message.GetCurrentSpecifier(&index, &specifier, &what, &name) != B_OK)
    return false;
  const auto property = FindProperty(name);
  if (property == Unknown)
    return false;

  BMessage reply(B_REPLY);
  reply.AddInt32("error", ReplyToProperty(message, property, workspace, source,
                                          destination, queue, reply));
  message.SendReply(&reply);
  return true;
}
}
