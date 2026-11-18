// SPDX-License-Identifier: MIT
#pragma once
#include "InspectorContent.h"
#include <File.h>
#include <string>

namespace Inspector {
constexpr size_t kDocumentLimit = TextLimit;
// A complete local source, not a potentially truncated preview string.
class Document {
public:
  explicit Document(const std::string &path);
  Document(const Document &) = delete;
  Document &operator=(const Document &) = delete;
  const std::string &Text() const { return text; }
  void Save(const std::string &text);

private:
  BFile source;
  std::string path, original, text;
  bool bom = false, crlf = false;
  void Guard() const;
};
}
