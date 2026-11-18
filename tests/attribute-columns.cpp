// SPDX-License-Identifier: MIT
#include "FileProperties.h"
#include <Application.h>
#include <fs_index.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Listi;
namespace {
void Check(bool ok, const char *msg) {
  if (!ok)
    throw std::runtime_error(msg);
  std::cout << "PASS " << msg << std::endl;
}
struct Fixture {
  std::string folder, file, name;
  dev_t device;
  bool ownsIndex = false;
  Fixture() {
    char pattern[] = "/boot/home/listi-index-XXXXXX";
    if (!mkdtemp(pattern))
      throw std::runtime_error("mkdtemp");
    folder = pattern;
    file = folder + "/original";
    name = "Listi:" + folder.substr(folder.rfind('/') + 1);
    std::ofstream(file) << "unchanged";
    struct stat st;
    if (stat(file.c_str(), &st))
      throw std::runtime_error("stat");
    device = st.st_dev;
    index_info info;
    if (fs_stat_index(device, name.c_str(), &info) == 0 || errno != ENOENT)
      throw std::runtime_error("index fixture name unavailable");
  }
  ~Fixture() {
    if (ownsIndex)
      fs_remove_index(device, name.c_str());
    unlink(file.c_str());
    rmdir(folder.c_str());
  }
};
FileAttribute Read(PropertySession &session, const std::string &name) {
  for (auto attr : session.Read().attributes)
    if (attr.name == name)
      return attr;
  throw std::runtime_error("missing fixture attribute");
}
}
int main() {
  BApplication app("application/x-vnd.Listi-AttributeColumnsTest");
  try {
    Fixture f;
    BNode node(f.file.c_str());
    PropertySession session(f.file);
    Check(AttributeCell(node, f.name.c_str()) == "(missing)",
          "missing value displayed");
    session.Save(nullptr, f.name, B_STRING_TYPE, "");
    Check(AttributeCell(node, f.name.c_str()) == "(empty)",
          "empty string differs from missing");
    auto expected = Read(session, f.name);
    session.Save(&expected, f.name, B_STRING_TYPE, "fresh");
    bool refused = false;
    try {
      session.MakeSearchable(expected);
    } catch (const std::exception &) {
      refused = true;
    }
    index_info info;
    Check(refused && fs_stat_index(f.device, f.name.c_str(), &info) != 0,
          "stale selection creates no index");
    expected = Read(session, f.name);
    f.ownsIndex = true;
    auto result = session.MakeSearchable(expected);
    std::cout << "PREEXISTING " << result << std::endl;
    Check(result.find("Index ready.") == 0 &&
              fs_stat_index(f.device, f.name.c_str(), &info) == 0 &&
              info.type == B_STRING_TYPE,
          "created matching volume index");
    Check(AttributeCell(node, f.name.c_str()) == "fresh",
          "index operation does not rewrite attribute");
    session.Save(&expected, f.name, B_STRING_TYPE, "after-index");
    expected = Read(session, f.name);
    Check(session.MakeSearchable(expected).find("found by a volume query") !=
              std::string::npos,
          "updated value resolves by index and existing matching index reused");
    session.Remove(expected);
    session.Save(nullptr, f.name, B_INT32_TYPE, "-91");
    Check(AttributeCell(node, f.name.c_str()) == "-91",
          "signed numeric column decoded");
    refused = false;
    try {
      session.MakeSearchable(Read(session, f.name));
    } catch (const std::exception &) {
      refused = true;
    }
    Check(refused && fs_stat_index(f.device, f.name.c_str(), &info) == 0 &&
              info.type == B_STRING_TYPE,
          "wrong-type index is not replaced");
    Check(!IndexableAttribute(B_BOOL_TYPE) && !IndexableAttribute(B_RAW_TYPE),
          "unsupported index types excluded");
    Check(Read(session, f.name).text == "-91",
          "original metadata intact after refusal");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << std::endl;
    return 1;
  }
}
