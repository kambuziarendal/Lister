// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace Inspector {
namespace Syntax {
using View = std::string_view;
enum class Family { Plain, Code, Script, Sql, Markup, Markdown };
enum class Ink { Text, Comment, String, Keyword, Number };
struct Run {
  size_t offset;
  Ink ink;
};

inline bool OneOf(View word, std::initializer_list<View> values) {
  return std::find(values.begin(), values.end(), word) != values.end();
}

inline Family Language(const std::string &path, View body) {
  std::string name = path.substr(path.rfind('/') + 1);
  for (char &c : name)
    c = std::tolower(static_cast<unsigned char>(c));
  const size_t dot = name.rfind('.');
  const View ext =
      dot == std::string::npos ? View() : View(name).substr(dot + 1);
  if (OneOf(ext, {"md", "markdown", "mdown", "mkd"}))
    return Family::Markdown;
  if (OneOf(ext, {"html", "htm", "xhtml", "xml", "svg"}))
    return Family::Markup;
  if (OneOf(ext, {"sql", "lua"}))
    return Family::Sql;
  if (OneOf(ext, {"sh", "bash", "zsh", "fish", "py", "rb", "pl", "yml", "yaml",
                  "toml", "cmake"}) ||
      OneOf(name, {"makefile", "dockerfile", "cmakelists.txt"}))
    return Family::Script;
  if (OneOf(ext, {"c", "cc", "cpp", "cxx", "h", "hh", "hpp", "js", "jsx", "ts",
                  "tsx", "css", "java", "kt", "go", "rs", "json", "swift"}))
    return Family::Code;
  return body.substr(0, 2) == "#!" ? Family::Script : Family::Plain;
}

inline std::vector<Run> ColourRuns(View text, Family family) {
  std::vector<Run> runs{{0, Ink::Text}};
  auto colour = [&](size_t begin, size_t end, Ink ink) {
    if (begin == runs.back().offset)
      runs.back().ink = ink;
    else
      runs.push_back({begin, ink});
    if (end < text.size())
      runs.push_back({end, Ink::Text});
  };
  auto through = [&](size_t from, View marker) {
    size_t at = text.find(marker, from);
    return at == View::npos ? text.size() : at + marker.size();
  };
  auto lineEnd = [&](size_t from) {
    return std::min(text.find('\n', from), text.size());
  };
  auto quoted = [&](size_t at) {
    const char quote = text[at++];
    while (at < text.size()) {
      const char c = text[at++];
      if (c == quote || c == '\n')
        break;
      if (c == '\\' && at < text.size())
        ++at;
    }
    return at;
  };
  if (family == Family::Plain)
    return runs;
  bool inTag = false;
  for (size_t i = 0; i < text.size();) {
    const size_t begin = i;
    const unsigned char c = text[i++];
    const View tail = text.substr(begin);
    if (family == Family::Markdown) {
      if ((begin == 0 || text[begin - 1] == '\n') && (c == '#' || c == '>')) {
        i = lineEnd(i);
        colour(begin, i, c == '#' ? Ink::Keyword : Ink::Comment);
      } else if (tail.substr(0, 3) == "```") {
        i = through(begin + 3, "```");
        colour(begin, i, Ink::String);
      } else if (c == '`') {
        const size_t close = text.find_first_of("`\n", i);
        i = close == View::npos ? text.size() : close + (text[close] == '`');
        colour(begin, i, Ink::String);
      }
      continue;
    }
    if (family == Family::Markup && tail.substr(0, 4) == "<!--") {
      i = through(i + 3, "-->");
      colour(begin, i, Ink::Comment);
    } else if (family == Family::Markup && c == '<') {
      inTag = true;
      while (i < text.size() && text[i] != '>' &&
             !std::isspace(static_cast<unsigned char>(text[i])))
        ++i;
      colour(begin, i, Ink::Keyword);
    } else if (family == Family::Markup && c == '>') {
      inTag = false;
      colour(begin, i, Ink::Keyword);
    } else if (family == Family::Markup && !inTag) {
      continue;
    } else if ((family == Family::Code && tail.substr(0, 2) == "//") ||
               (family == Family::Script && c == '#') ||
               (family == Family::Sql && tail.substr(0, 2) == "--")) {
      i = lineEnd(i);
      colour(begin, i, Ink::Comment);
    } else if ((family == Family::Code || family == Family::Sql) &&
               tail.substr(0, 2) == "/*") {
      i = through(i + 1, "*/");
      colour(begin, i, Ink::Comment);
    } else if (c == '"' || c == '\'' || c == '`') {
      i = quoted(begin);
      colour(begin, i, Ink::String);
    } else if (std::isalnum(c) || c == '_') {
      while (
          i < text.size() &&
          (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
        ++i;
      if (std::isdigit(c))
        colour(begin, i, Ink::Number);
      else if (OneOf(text.substr(begin, i - begin),
                     {"if",     "else",   "elif",    "then",      "fi",
                      "do",     "done",   "for",     "in",        "while",
                      "case",   "switch", "break",   "continue",  "class",
                      "struct", "def",    "fn",      "function",  "return",
                      "import", "from",   "include", "namespace", "const",
                      "let",    "var",    "void",    "int",       "bool",
                      "auto",   "true",   "false",   "null",      "None",
                      "True",   "False",  "export",  "local",     "end",
                      "select", "SELECT", "FROM",    "WHERE",     "INSERT",
                      "UPDATE"}))
        colour(begin, i, Ink::Keyword);
    }
  }
  return runs;
}
}
}
