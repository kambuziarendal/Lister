// SPDX-License-Identifier: MIT
// Interface declarations in vendor/p7zip retain their upstream LGPL license.
#define WINAPI
#define INITGUID
#include "ArchiveBackend.h"
#include "ArchivePasswords.h"
#include "DynamicLibrary.h"
#include "vendor/p7zip/CPP/7zip/Archive/IArchive.h"
#include "vendor/p7zip/CPP/7zip/ICoder.h"
#include "vendor/p7zip/CPP/7zip/IPassword.h"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace Lister {
namespace {
void Require(bool value, const char *error) {
  if (!value)
    throw std::runtime_error(error);
}
#define REFS                                                                   \
  std::atomic<ULONG> refs{1};                                                  \
  ULONG AddRef() override { return ++refs; }                                   \
  ULONG Release() override {                                                   \
    auto n = --refs;                                                           \
    if (!n)                                                                    \
      delete this;                                                             \
    return n;                                                                  \
  }
bool Equal(const GUID &a, const GUID &b) { return !memcmp(&a, &b, sizeof(a)); }
using CountFn = HRESULT (*)(UInt32 *);
using PropertyFn = HRESULT (*)(UInt32, PROPID, PROPVARIANT *);
using CreateFn = HRESULT (*)(const GUID *, const GUID *, void **);
using CodecFn = HRESULT (*)(UInt32, const GUID *, void **);
using SetCodecsFn = HRESULT (*)(ICompressCodecsInfo *);
struct CodecInfo : ICompressCodecsInfo {
  DynamicLibrary lib{"/boot/system/lib/p7zip/Codecs/Rar.so"};
  REFS HRESULT QueryInterface(REFIID id, void **out) override {
    *out = nullptr;
    if (!Equal(id, IID_IUnknown) && !Equal(id, IID_ICompressCodecsInfo))
      return E_NOINTERFACE;
    *out = static_cast<ICompressCodecsInfo *>(this);
    AddRef();
    return S_OK;
  }
  HRESULT GetNumMethods(UInt32 *n) override {
    return lib.Function<CountFn>("GetNumberOfMethods")(n);
  }
  HRESULT GetProperty(UInt32 i, PROPID p, PROPVARIANT *v) override {
    return lib.Function<PropertyFn>("GetMethodProperty")(i, p, v);
  }
  HRESULT CreateDecoder(UInt32 i, const GUID *id, void **out) override {
    return lib.Function<CodecFn>("CreateDecoder")(i, id, out);
  }
  HRESULT CreateEncoder(UInt32, const GUID *, void **out) override {
    *out = nullptr;
    return E_NOINTERFACE;
  }
};
struct Module {
  DynamicLibrary lib{"/boot/system/lib/p7zip/7z.so"};
  CodecInfo *codecs = nullptr;
  std::map<std::string, GUID> formats;
  Module() {
    if (!lib.Available())
      return;
    UInt32 count = 0;
    Require(lib.Function<CountFn>("GetNumberOfFormats")(&count) == S_OK &&
                count < 1024,
            "Unsupported p7zip interface");
    auto prop = lib.Function<PropertyFn>("GetHandlerProperty2");
    for (UInt32 i = 0; i < count; ++i) {
      PROPVARIANT name{};
      Require(prop(i, NArchive::NHandlerPropID::kName, &name) == S_OK,
              "Cannot inspect p7zip format");
      std::string key;
      if (name.vt == VT_BSTR) {
        if (!wcscmp(name.bstrVal, L"7z"))
          key = "7z";
        if (!wcscmp(name.bstrVal, L"Rar"))
          key = "rar";
        if (!wcscmp(name.bstrVal, L"Rar5"))
          key = "rar5";
      }
      lib.Function<decltype(&VariantClear)>("VariantClear")(&name);
      if (key.empty())
        continue;
      PROPVARIANT cls{};
      Require(prop(i, NArchive::NHandlerPropID::kClassID, &cls) == S_OK &&
                  cls.vt == VT_BSTR,
              "Cannot inspect p7zip class");
      GUID id;
      memcpy(&id, cls.bstrVal, sizeof(id));
      formats[key] = id;
      lib.Function<decltype(&VariantClear)>("VariantClear")(&cls);
    }
    codecs = new CodecInfo;
    if (codecs->lib.Available()) {
      codecs->lib.Function<CountFn>("GetNumberOfMethods");
      codecs->lib.Function<PropertyFn>("GetMethodProperty");
      codecs->lib.Function<CodecFn>("CreateDecoder");
      Require(lib.Function<SetCodecsFn>("SetCodecs")(codecs) == S_OK,
              "Cannot load optional RAR codecs");
    }
  }
  ~Module() {
    if (codecs) {
      if (codecs->lib.Available())
        lib.Function<SetCodecsFn>("SetCodecs")(nullptr);
      codecs->Release();
    }
  }
};
std::shared_ptr<Module> Engine() {
  // Keep the loaded plugin alive until process teardown. Its C++ globals can
  // be finalized before function-local app statics; calling SetCodecs/dlclose
  // from a later app static destructor would touch already-destroyed globals.
  // This is one process-lifetime module, not a per-reader allocation.
  struct Cache {
    std::mutex mutex;
    std::shared_ptr<Module> module;
  };
  static Cache *cache = new Cache;
  std::lock_guard<std::mutex> lock(cache->mutex);
  if (!cache->module || !cache->module->lib.Available())
    cache->module = std::make_shared<Module>();
  return cache->module;
}
std::wstring Wide(const char *text) {
  std::wstring out;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
  while (*p) {
    uint32_t code = *p++;
    unsigned n = 0;
    if (code >= 0xf0) {
      Require(code <= 0xf4, "Invalid UTF-8 password");
      code &= 7;
      n = 3;
    } else if (code >= 0xe0) {
      code &= 15;
      n = 2;
    } else if (code >= 0xc2) {
      code &= 31;
      n = 1;
    } else
      Require(code < 128, "Invalid UTF-8 password");
    for (unsigned i = 0; i < n; ++i) {
      Require((*p & 0xc0) == 0x80, "Invalid UTF-8 password");
      code = (code << 6) | (*p++ & 63);
    }
    Require(code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff) &&
                (!n || code >= (n == 1   ? 128
                                : n == 2 ? 2048
                                         : 65536)),
            "Invalid UTF-8 password");
    out.push_back(wchar_t(code));
  }
  return out;
}
std::string UTF8(BSTR str) {
  std::string out;
  if (!str)
    return out;
  for (size_t i = 0; str[i]; ++i) {
    Require(i < 32768, "Archive path is too long");
    uint32_t c = str[i];
    Require(c <= 0x10ffff && !(c >= 0xd800 && c <= 0xdfff),
            "Archive contains invalid Unicode");
    if (c < 128)
      out += char(c);
    else if (c < 2048) {
      out += char(0xc0 | (c >> 6));
      out += char(0x80 | (c & 63));
    } else if (c < 65536) {
      out += char(0xe0 | (c >> 12));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else {
      out += char(0xf0 | (c >> 18));
      out += char(0x80 | ((c >> 12) & 63));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    }
  }
  return out;
}
struct State {
  std::shared_ptr<Module> engine = Engine();
  std::string path, identity, format;
  const TransferControl &control;
  int fd = -1;
  uint64_t generation = ArchivePasswordGeneration();
  ArchiveSecretPtr secret;
  bool retry = false, requested = false;
  std::mutex mutex;
  std::exception_ptr failure;
  State(const std::string &p, const TransferControl &c) : path(p), control(c) {
    Require(engine->lib.Available(),
            "Install the optional p7zip package from HaikuDepot, then reopen "
            "this archive in Listi.");
    fd = open(p.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    Require(fd >= 0, "Cannot open archive");
    try {
      struct stat st;
      Require(!fstat(fd, &st) && S_ISREG(st.st_mode),
              "Archive must be a regular file");
      identity = ArchivePasswordIdentity(st);
      format = SevenZipFormat(p);
      Require(engine->formats.count(format),
              "The p7zip engine does not support this archive");
    } catch (...) {
      close(fd);
      fd = -1;
      throw;
    }
  }
  ~State() {
    if (fd >= 0)
      close(fd);
  }
  void Check() { control.Check(); }
  void Verify() {
    struct stat st, named;
    Require(!fstat(fd, &st) && !lstat(path.c_str(), &named) &&
                identity == ArchivePasswordIdentity(st) &&
                identity == ArchivePasswordIdentity(named),
            "Archive changed during reading or password entry. Open it again.");
  }
  template <typename F> HRESULT Call(F fn) {
    try {
      Check();
      fn();
      return S_OK;
    } catch (...) {
      std::lock_guard<std::mutex> lock(mutex);
      if (!failure)
        failure = std::current_exception();
      return E_ABORT;
    }
  }
  void Rethrow() {
    std::lock_guard<std::mutex> lock(mutex);
    if (failure)
      std::rethrow_exception(failure);
  }
  HRESULT Password(BSTR *out) {
    *out = nullptr;
    return Call([&] {
      if (!secret)
        secret = GetArchivePassword(path, identity, retry, generation, control);
      requested = true;
      Verify();
      auto wide = Wide(secret->Text());
      *out = engine->lib.Function<decltype(&SysAllocStringLen)>(
          "SysAllocStringLen")(wide.data(), wide.size());
      volatile wchar_t *wipe = wide.data();
      for (size_t i = 0; i < wide.size(); ++i)
        wipe[i] = 0;
      Require(*out, "Cannot allocate password");
    });
  }
};
struct Input : IInStream {
  State &s;
  explicit Input(State &state) : s(state) {}
  REFS HRESULT QueryInterface(REFIID id, void **out) override {
    *out = nullptr;
    if (Equal(id, IID_IUnknown) || Equal(id, IID_IInStream))
      *out = static_cast<IInStream *>(this);
    else if (Equal(id, IID_ISequentialInStream))
      *out = static_cast<ISequentialInStream *>(this);
    else
      return E_NOINTERFACE;
    AddRef();
    return S_OK;
  }
  HRESULT Read(void *data, UInt32 size, UInt32 *n) override {
    if (n)
      *n = 0;
    return s.Call([&] {
      ssize_t got;
      do {
        got = read(s.fd, data, size);
      } while (got < 0 && errno == EINTR);
      Require(got >= 0, "Archive read failed");
      if (n)
        *n = got;
    });
  }
  HRESULT Seek(Int64 offset, UInt32 origin, UInt64 *position) override {
    return s.Call([&] {
      Require(origin <= 2, "Invalid archive seek");
      auto pos = lseek(s.fd, offset, origin);
      Require(pos >= 0, "Archive seek failed");
      if (position)
        *position = pos;
    });
  }
};
struct OpenCallback : IArchiveOpenCallback, ICryptoGetTextPassword {
  State &s;
  explicit OpenCallback(State &state) : s(state) {}
  REFS HRESULT QueryInterface(REFIID id, void **out) override {
    *out = nullptr;
    if (Equal(id, IID_IUnknown) || Equal(id, IID_IArchiveOpenCallback))
      *out = static_cast<IArchiveOpenCallback *>(this);
    else if (Equal(id, IID_ICryptoGetTextPassword))
      *out = static_cast<ICryptoGetTextPassword *>(this);
    else
      return E_NOINTERFACE;
    AddRef();
    return S_OK;
  }
  HRESULT SetTotal(const UInt64 *, const UInt64 *) override {
    return s.Call([] {});
  }
  HRESULT SetCompleted(const UInt64 *, const UInt64 *) override {
    return s.Call([] {});
  }
  HRESULT CryptoGetTextPassword(BSTR *out) override { return s.Password(out); }
};
struct Variant {
  Module &m;
  PROPVARIANT value{};
  explicit Variant(Module &mod) : m(mod) {}
  ~Variant() {
    m.lib.Function<decltype(&VariantClear)>("VariantClear")(&value);
  }
};
struct Archive {
  State &s;
  IInArchive *archive = nullptr;
  Input *input = nullptr;
  OpenCallback *callback = nullptr;
  explicit Archive(State &state) : s(state) {
    Require(s.engine->lib.Function<CreateFn>("CreateObject")(
                &s.engine->formats.at(s.format), &IID_IInArchive,
                reinterpret_cast<void **>(&archive)) == S_OK &&
                archive,
            "Cannot create archive reader");
    input = new Input(s);
    callback = new OpenCallback(s);
  }
  ~Archive() {
    if (archive) {
      archive->Close();
      archive->Release();
    }
    if (callback)
      callback->Release();
    if (input)
      input->Release();
  }
  bool Open() {
    s.Check();
    s.Verify();
    s.requested = false;
    Require(lseek(s.fd, 0, SEEK_SET) == 0, "Cannot rewind archive");
    UInt64 limit = 0;
    auto result = archive->Open(input, &limit, callback);
    s.Rethrow();
    return result == S_OK;
  }
  void Property(UInt32 index, PROPID property, Variant &v) {
    Require(archive->GetProperty(index, property, &v.value) == S_OK,
            "Cannot read archive metadata");
  }
  bool Bool(UInt32 index, PROPID property) {
    Variant v(*s.engine);
    Property(index, property, v);
    return v.value.vt == VT_BOOL && v.value.boolVal;
  }
  std::vector<FileEntry> Entries() {
    UInt32 count = 0;
    Require(archive->GetNumberOfItems(&count) == S_OK && count <= 200000,
            "Archive has too many entries");
    Variant volume(*s.engine);
    Require(archive->GetArchiveProperty(kpidIsVolume, &volume.value) == S_OK,
            "Cannot read archive volume state");
    Require(volume.value.vt != VT_BOOL || !volume.value.boolVal,
            "Multipart archives are not supported yet; nothing was extracted.");
    std::vector<FileEntry> out;
    out.reserve(count);
    std::set<std::string> names;
    for (UInt32 i = 0; i < count; ++i) {
      s.Check();
      FileEntry e;
      Variant path(*s.engine);
      Property(i, kpidPath, path);
      Require(path.value.vt == VT_BSTR, "Archive entry has no name");
      e.name = SafeArchivePath(UTF8(path.value.bstrVal).c_str());
      Require(!e.name.empty() && names.insert(e.name).second,
              "Archive contains empty or duplicate entry paths");
      e.path = e.name;
      e.directory = Bool(i, kpidIsDir);
      e.link = false;
      for (auto property : {kpidSymLink, kpidHardLink, kpidCopyLink}) {
        Variant link(*s.engine);
        Property(i, property, link);
        if (link.value.vt == VT_BSTR && link.value.bstrVal &&
            link.value.bstrVal[0])
          e.link = true;
      }
      Require(!Bool(i, kpidIsAnti) && !Bool(i, kpidIsAltStream) &&
                  !Bool(i, kpidSplitBefore) && !Bool(i, kpidSplitAfter),
              "Archive contains unsupported deleted entries, streams or split "
              "volumes");
      Variant size(*s.engine);
      Property(i, kpidSize, size);
      e.sizeKnown = size.value.vt == VT_UI8;
      if (e.sizeKnown) {
        Require(size.value.uhVal.QuadPart <= uint64_t(INT64_MAX),
                "Archive member is too large");
        e.size = size.value.uhVal.QuadPart;
      }
      e.permissions = e.directory ? 0755 : 0644;
      Variant mode(*s.engine);
      Property(i, kpidPosixAttrib, mode);
      unsigned bits = mode.value.vt == VT_UI4 ? mode.value.ulVal : 0;
      Variant attr(*s.engine);
      Property(i, kpidAttrib, attr);
      // p7zip encodes UNIX modes in the high word of Win32 attributes.
      if (!bits && attr.value.vt == VT_UI4 && (attr.value.ulVal & 0x8000))
        bits = attr.value.ulVal >> 16;
      if (attr.value.vt == VT_UI4 && (attr.value.ulVal & 0x400))
        e.link = true;
      if (bits) {
        e.permissions = bits & 0777;
        e.link |= S_ISLNK(bits);
        Require(!(bits & S_IFMT) || S_ISDIR(bits) || S_ISREG(bits) ||
                    S_ISLNK(bits),
                "Archive contains a special device entry");
      }
      Variant time(*s.engine);
      Property(i, kpidMTime, time);
      if (time.value.vt == VT_FILETIME) {
        uint64_t ticks = (uint64_t(time.value.filetime.dwHighDateTime) << 32) |
                         time.value.filetime.dwLowDateTime;
        if (ticks >= 116444736000000000ULL)
          e.modified = (ticks - 116444736000000000ULL) / 10000000;
      }
      out.push_back(e);
    }
    s.Verify();
    return out;
  }
};
struct Output : ISequentialOutStream {
  State &s;
  int fd;
  uint64_t bytes = 0, limit;
  Output(State &state, int dest, uint64_t maximum)
      : s(state), fd(dest), limit(maximum) {}
  REFS HRESULT QueryInterface(REFIID id, void **out) override {
    *out = nullptr;
    if (!Equal(id, IID_IUnknown) && !Equal(id, IID_ISequentialOutStream))
      return E_NOINTERFACE;
    *out = static_cast<ISequentialOutStream *>(this);
    AddRef();
    return S_OK;
  }
  HRESULT Write(const void *data, UInt32 size, UInt32 *done) override {
    if (done)
      *done = 0;
    return s.Call([&] {
      Require(bytes <= limit && size <= limit - bytes,
              "Archive member exceeds its declared size");
      const char *p = static_cast<const char *>(data);
      size_t left = size;
      while (left) {
        s.Check();
        auto n = write(fd, p, left);
        if (n < 0 && errno == EINTR)
          continue;
        Require(n > 0,
                "Cannot stage decoded archive data (check free disk space)");
        p += n;
        left -= n;
      }
      bytes += size;
      if (done)
        *done = size;
      s.control.Tick(0);
    });
  }
};
struct ExtractCallback : IArchiveExtractCallback, ICryptoGetTextPassword {
  State &s;
  Output *output;
  UInt32 wanted;
  int result = -1;
  bool prepared = false;
  ExtractCallback(State &state, Output *out, UInt32 index)
      : s(state), output(out), wanted(index) {
    output->AddRef();
  }
  ~ExtractCallback() { output->Release(); }
  REFS HRESULT QueryInterface(REFIID id, void **out) override {
    *out = nullptr;
    if (Equal(id, IID_IUnknown) || Equal(id, IID_IArchiveExtractCallback))
      *out = static_cast<IArchiveExtractCallback *>(this);
    else if (Equal(id, IID_IProgress))
      *out = static_cast<IProgress *>(this);
    else if (Equal(id, IID_ICryptoGetTextPassword))
      *out = static_cast<ICryptoGetTextPassword *>(this);
    else
      return E_NOINTERFACE;
    AddRef();
    return S_OK;
  }
  HRESULT SetTotal(UInt64) override {
    return s.Call([] {});
  }
  HRESULT SetCompleted(const UInt64 *) override {
    return s.Call([] {});
  }
  HRESULT GetStream(UInt32 index, ISequentialOutStream **out,
                    Int32 mode) override {
    *out = nullptr;
    return s.Call([&] {
      prepared =
          index == wanted && mode == NArchive::NExtract::NAskMode::kExtract;
      if (prepared) {
        output->AddRef();
        *out = output;
      }
    });
  }
  HRESULT PrepareOperation(Int32) override {
    return s.Call([] {});
  }
  HRESULT SetOperationResult(Int32 value) override {
    if (prepared)
      result = value;
    return s.Call([] {});
  }
  bool SetFileSymLinkAttrib() override { return false; }
  HRESULT CryptoGetTextPassword(BSTR *out) override { return s.Password(out); }
};
struct Scratch {
  int fd = -1;
  Scratch() {
    char path[] = "/tmp/Listi-decode-XXXXXX";
    fd = mkstemp(path);
    Require(fd >= 0, "Cannot stage decoded archive data");
    if (unlink(path) != 0) {
      close(fd);
      fd = -1;
      throw std::runtime_error("Cannot protect decoded archive staging");
    }
  }
  ~Scratch() {
    if (fd >= 0)
      close(fd);
  }
};
void Retry(State &s, unsigned attempt) {
  Require(s.requested && attempt < 3,
          "Wrong password, damaged archive or unsupported archive method.");
  s.secret.reset();
  s.retry = true;
}
std::vector<FileEntry> Contents(const std::string &path,
                                const TransferControl &c) {
  State state(path, c);
  for (unsigned attempt = 0;; ++attempt) {
    Archive archive(state);
    if (!archive.Open()) {
      Retry(state, attempt);
      continue;
    }
    auto entries = archive.Entries();
    RememberArchivePassword(state.identity, state.secret, state.generation);
    return entries;
  }
}
} // namespace
bool SevenZipAvailable() { return Engine()->lib.Available(); }
std::string SevenZipFormat(const std::string &path) {
  int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0)
    return "";
  unsigned char bytes[8]{};
  struct stat st;
  ssize_t n = 0;
  if (!fstat(fd, &st) && S_ISREG(st.st_mode))
    n = read(fd, bytes, sizeof(bytes));
  close(fd);
  if (n >= 6 && !memcmp(bytes, "7z\xbc\xaf\x27\x1c", 6))
    return "7z";
  if (n >= 7 && !memcmp(bytes, "Rar!\x1a\x07\0", 7))
    return "rar";
  if (n >= 8 && !memcmp(bytes, "Rar!\x1a\x07\x01\0", 8))
    return "rar5";
  return "";
}
std::string SevenZipOpen(const std::string &path, const TransferControl &c) {
  Contents(path, c);
  Location loc;
  loc.scheme = "archive";
  loc.archive = path;
  loc.path = "/";
  return loc.URL();
}
std::vector<FileEntry> SevenZipList(const Location &location,
                                    const TransferControl &c) {
  auto entries = Contents(location.archive, c);
  std::map<std::string, FileEntry> children;
  auto prefix = location.path == "/" ? "" : location.path.substr(1) + "/";
  for (const auto &e : entries) {
    if (e.name.compare(0, prefix.size(), prefix))
      continue;
    auto rest = e.name.substr(prefix.size());
    if (rest.empty())
      continue;
    auto slash = rest.find('/');
    auto leaf = rest.substr(0, slash);
    auto &child = children[leaf];
    if (slash == std::string::npos) {
      Require(!child.directory || e.directory,
              "Archive has conflicting file and folder paths");
      child = e;
    } else {
      Require(child.name.empty() || child.directory,
              "Archive has conflicting file and folder paths");
      child.directory = true;
      child.permissions = 0755;
    }
    child.name = leaf;
    child.path = ChildLocation(location.URL(), leaf);
  }
  std::vector<FileEntry> out;
  for (auto &item : children)
    out.push_back(item.second);
  return out;
}
void SevenZipRead(const Location &location, const DataSink &sink,
                  const TransferControl &c) {
  State state(location.archive, c);
  for (unsigned attempt = 0;; ++attempt) {
    Archive archive(state);
    if (!archive.Open()) {
      Retry(state, attempt);
      continue;
    }
    auto entries = archive.Entries();
    UInt32 index = 0;
    while (index < entries.size() &&
           entries[index].name != location.path.substr(1))
      ++index;
    Require(index < entries.size(), "Archive entry no longer exists");
    const auto &entry = entries[index];
    Require(!entry.link && !entry.directory && entry.sizeKnown,
            "Archive links, directories and unknown-size entries are not "
            "extracted");
    Scratch scratch;
    auto out = new Output(state, scratch.fd, entry.size);
    auto callback = new ExtractCallback(state, out, index);
    auto result = archive.archive->Extract(&index, 1, 0, callback);
    auto operation = callback->result;
    auto bytes = out->bytes;
    callback->Release();
    out->Release();
    state.Rethrow();
    state.Verify();
    if (result != S_OK ||
        operation != NArchive::NExtract::NOperationResult::kOK) {
      Require(operation !=
                  NArchive::NExtract::NOperationResult::kUnsupportedMethod,
              "Unsupported method. Install the complete p7zip package with its "
              "RAR codec.");
      Retry(state, attempt);
      continue;
    }
    Require(bytes == uint64_t(entry.size),
            "Archive member has an unexpected decoded size");
    RememberArchivePassword(state.identity, state.secret, state.generation);
    Require(lseek(scratch.fd, 0, SEEK_SET) == 0,
            "Cannot rewind decoded archive data");
    std::vector<char> buffer(131072);
    for (;;) {
      c.Check();
      ssize_t n;
      do {
        n = read(scratch.fd, buffer.data(), buffer.size());
      } while (n < 0 && errno == EINTR);
      Require(n >= 0, "Cannot read decoded archive data");
      if (!n)
        break;
      sink(buffer.data(), n);
      c.Tick(n);
    }
    return;
  }
}
} // namespace Lister
