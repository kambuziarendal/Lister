// SPDX-License-Identifier: MIT
#pragma once
#include <Node.h>
#include <memory>
#include <string>
#include <vector>
#include <sys/stat.h>
namespace Listi {
struct FileAttribute {
  std::string name, text;
  type_code type = 0;
  off_t size = 0;
  bool editable = false;
  std::vector<uint8> bytes;
};
struct PropertySnapshot {
  std::string path, summary;
  struct stat stat {};
  std::vector<FileAttribute> attributes;
  bool limited = false, writableKind = false;
};
// Worker-side bounded preview for an optional Details column.
std::string AttributeCell(BNode &, const char *name, type_code *type = nullptr);
bool IndexableAttribute(type_code);
std::string AttributeTypeName(type_code);
std::vector<uint8> EncodeAttribute(type_code, const std::string &);
// A session pins one native node, and refuses writes after its pathname changes
// identity. Opening or refreshing never follows a symbolic link for editing.
class PropertySession {
public:
  explicit PropertySession(const std::string &);
  PropertySnapshot Read();
  void Save(const FileAttribute *expected, const std::string &name,
            type_code type, const std::string &text);
  void Remove(const FileAttribute &expected);
  std::string MakeSearchable(const FileAttribute &expected);
  void Permissions(mode_t expected, const std::string &octal);

private:
  std::string path;
  BNode node;
  node_ref identity;
  void Guard();
  FileAttribute Attribute(const std::string &name);
  void Change(const FileAttribute *, const std::string &, type_code,
              const std::vector<uint8> *, bool remove);
};
}
