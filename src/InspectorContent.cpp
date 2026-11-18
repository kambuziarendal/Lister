// SPDX-License-Identifier: MIT
#include "InspectorContent.h"
#include "ImagePreview.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <MediaFile.h>
#include <MediaRoster.h>
#include <MediaTrack.h>
#include <Sound.h>
#include <SoundPlayer.h>
#include <String.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;
namespace Inspector {
namespace {
int OpenRegular(const std::string &path) {
  struct stat st;
  if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
    return -1;
  return open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
}
} // namespace

Input::Input(const std::string &path, off_t limit)
    : fd(OpenRegular(path)) {
  struct stat st;
  if (fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
      st.st_size >= 0 && st.st_size <= limit) {
    length = st.st_size;
    return;
  }
  if (fd >= 0)
    close(fd);
  throw std::runtime_error("Preview requires a readable regular file within the size limit");
}

Input::~Input() { close(fd); }
ssize_t Input::ReadAt(off_t offset, void *data, size_t size) {
  if (offset < 0)
    return B_BAD_VALUE;
  if (offset >= length)
    return 0;
  ssize_t count;
  do {
    count = pread(fd, data, std::min<size_t>(size, length - offset), offset);
  } while (count < 0 && errno == EINTR);
  return count;
}
ssize_t Input::WriteAt(off_t, const void *, size_t) { return B_NOT_ALLOWED; }
off_t Input::Seek(off_t offset, uint32 mode) {
  off_t base = mode == SEEK_SET ? 0 : mode == SEEK_CUR ? position : length;
  if ((mode != SEEK_SET && mode != SEEK_CUR && mode != SEEK_END) ||
      offset < -base || offset > INT64_MAX - base)
    return B_BAD_VALUE;
  return position = base + offset;
}
off_t Input::Position() const { return position; }
status_t Input::GetSize(off_t *size) const {
  *size = length;
  return B_OK;
}

namespace {
bool IsOneOf(std::string_view value,
             std::initializer_list<std::string_view> values) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

// Reject binary controls and invalid UTF-8. A cut-off trailing sequence is
// permitted only when the caller deliberately read a bounded prefix.
size_t Utf8Length(std::string_view text, bool prefix) {
  size_t i = 0;
  while (i < text.size()) {
    unsigned char first = text[i];
    if (first < 0x80) {
      if ((first < 32 && first != '\t' && first != '\n' && first != '\r' &&
           first != '\f') || first == 127)
        throw std::runtime_error("No text preview for binary content");
      ++i;
      continue;
    }
    size_t width = first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
    if (first < 0xc2 || first > 0xf4)
      throw std::runtime_error("Text preview requires UTF-8");
    if (width > text.size() - i && prefix)
      return i;
    for (size_t j = 1; j < width; ++j) {
      unsigned char min = 0x80, max = 0xbf;
      if (j == 1) {
        if (first == 0xe0) min = 0xa0;
        if (first == 0xed) max = 0x9f;
        if (first == 0xf0) min = 0x90;
        if (first == 0xf4) max = 0x8f;
      }
      if (i + j >= text.size() || static_cast<unsigned char>(text[i + j]) < min ||
          static_cast<unsigned char>(text[i + j]) > max)
        throw std::runtime_error("Text preview requires UTF-8");
    }
    i += width;
  }
  return i;
}

std::string ReadPrefix(Input &input, size_t limit) {
  std::string result(limit, '\0');
  size_t total = 0;
  while (total < limit) {
    ssize_t n = input.ReadAt(total, result.data() + total, limit - total);
    if (n < 0)
      throw std::runtime_error("Cannot read file");
    if (!n)
      break;
    total += n;
  }
  result.resize(total);
  return result;
}

class ScratchPdf {
public:
  ScratchPdf() {
    char pattern[] = "/tmp/Listi-pdf-XXXXXX";
    char *created = mkdtemp(pattern);
    if (!created)
      throw std::runtime_error("Cannot create temporary PDF preview directory");
    directory = created;
  }
  ~ScratchPdf() {
    unlink(Source().c_str());
    unlink(Image().c_str());
    rmdir(directory.c_str());
  }
  std::string Source() const { return directory + "/document.pdf"; }
  std::string Prefix() const { return directory + "/page"; }
  std::string Image() const { return Prefix() + ".png"; }
private:
  std::string directory;
};

void CopyPdf(Input &input, const std::string &destination,
             const Listi::TransferControl &control) {
  int fd = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0)
    throw std::runtime_error("Cannot create temporary PDF preview");
  try {
    constexpr off_t limit = 64 * 1024 * 1024;
    off_t length;
    input.GetSize(&length);
    if (length > limit)
      throw std::runtime_error("PDF previews are limited to 64 MiB");
    char buffer[64 * 1024];
    for (off_t offset = 0; offset < length;) {
      control.Check();
      ssize_t n = input.ReadAt(offset, buffer, sizeof(buffer));
      if (n <= 0)
        throw std::runtime_error("Cannot read PDF preview source");
      for (ssize_t written = 0; written < n;) {
        ssize_t count = write(fd, buffer + written, n - written);
        if (count < 0 && errno == EINTR)
          continue;
        if (count <= 0)
          throw std::runtime_error("Cannot write temporary PDF preview");
        written += count;
      }
      offset += n;
    }
  } catch (...) {
    close(fd);
    throw;
  }
  if (close(fd) < 0)
    throw std::runtime_error("Cannot write temporary PDF preview");
}

std::unique_ptr<BBitmap> ReadVideoFrame(BMediaTrack &track,
                                        const media_video_display_info &display) {
  auto frame = std::make_unique<BBitmap>(
      BRect(0, 0, display.line_width - 1, display.line_count - 1), B_RGB32);
  if (frame->InitCheck() != B_OK)
    throw std::runtime_error("Cannot allocate video preview");
  int64 frames = 1;
  if (frame->BytesPerRow() == static_cast<int32>(display.bytes_per_row)) {
    if (track.ReadFrames(frame->Bits(), &frames) != B_OK || frames != 1)
      return {};
  } else {
    std::vector<char> buffer(display.bytes_per_row * display.line_count);
    if (track.ReadFrames(buffer.data(), &frames) != B_OK || frames != 1)
      return {};
    auto *dest = static_cast<char *>(frame->Bits());
    for (uint32 y = 0; y < display.line_count; ++y)
      memcpy(dest + y * frame->BytesPerRow(),
             buffer.data() + y * display.bytes_per_row, display.line_width * 4);
  }
  return frame;
}

bool AudioOutputAvailable() {
  auto *roster = BMediaRoster::Roster();
  media_node output;
  if (!roster || roster->GetAudioOutput(&output) != B_OK)
    return false;
  roster->ReleaseNode(output);
  return true;
}

constexpr int kClipSeconds = 15;
constexpr bigtime_t kClipTime = kClipSeconds * 1000000LL;
struct SoundClip {
  std::vector<char> bytes;
  std::unique_ptr<BSoundPlayer> player;
  bigtime_t duration = 0;
  BSoundPlayer::play_id id = -1;
  void Start(BMediaTrack &track, const Listi::TransferControl &control) {
    media_format format{};
    format.type = B_MEDIA_RAW_AUDIO;
    format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
    format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
    if (track.DecodedFormat(&format) != B_OK)
      return;
    const auto &audio = format.u.raw_audio;
    if (!std::isfinite(audio.frame_rate) || audio.frame_rate < 1 ||
        audio.frame_rate > 192000 || !audio.channel_count || audio.channel_count > 8 ||
        audio.format != media_raw_audio_format::B_AUDIO_SHORT ||
        audio.byte_order != B_MEDIA_HOST_ENDIAN || !audio.buffer_size ||
        audio.buffer_size > 1024 * 1024)
      return;
    const size_t frameSize = 2 * audio.channel_count;
    size_t maxFrames = size_t(audio.frame_rate * kClipSeconds);
    std::vector<char> buffer(audio.buffer_size);
    int64 maxRead = buffer.size() / frameSize;
    while (maxFrames && maxRead) {
      control.Check();
      int64 frames = std::min<size_t>(maxFrames, maxRead);
      status_t result = track.ReadFrames(buffer.data(), &frames);
      if (frames <= 0 || frames > maxRead)
        break;
      frames = std::min<size_t>(frames, maxFrames);
      bytes.insert(bytes.end(), buffer.data(), buffer.data() + frames * frameSize);
      maxFrames -= frames;
      if (result != B_OK)
        break;
    }
    if (bytes.empty())
      return;
    control.Check();
    duration = static_cast<bigtime_t>(
        bytes.size() / frameSize * 1000000.0 / audio.frame_rate);
    auto release = [](BSound *sound) { sound->ReleaseRef(); };
    std::unique_ptr<BSound, decltype(release)> sound(
        new BSound(bytes.data(), bytes.size(), audio), release);
    if (sound->InitCheck() != B_OK)
      return;
    player = std::make_unique<BSoundPlayer>(&audio, "Listi inspector");
    if (player->InitCheck() == B_OK && player->Start() == B_OK)
      id = player->StartPlaying(sound.get());
    if (id < 0)
      player.reset();
  }
  ~SoundClip() {
    if (player) {
      player->StopPlaying(id);
      player->Stop(true, true);
    }
  }
};

void WaitUntil(bigtime_t deadline, const Listi::TransferControl &control) {
  while (system_time() < deadline) {
    control.Check();
    snooze(std::min<bigtime_t>(20000, deadline - system_time()));
  }
}
} // namespace

Kind Identify(const std::string &leaf, Input &input) {
  auto dot = leaf.rfind('.');
  std::string ext = dot == std::string::npos ? "" : leaf.substr(dot + 1);
  for (char &c : ext)
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  const auto bytes = ReadPrefix(input, 1024);
  if (bytes.rfind("%PDF-", 0) == 0 || ext == "pdf")
    return Kind::Pdf;
  if (IsOneOf(ext, {"png", "jpg", "jpeg", "gif", "bmp", "webp", "tif", "tiff",
                    "svg", "heic", "avif", "ico", "tga", "ppm", "pgm", "pbm"}))
    return Kind::Image;
  if (IsOneOf(ext, {"mp3", "wav", "flac", "aac", "aif", "aiff", "m4a", "oga",
                    "opus", "au", "wma"}))
    return Kind::Audio;
  if (IsOneOf(ext, {"mp4", "m4v", "mov", "mkv", "avi", "webm", "mpg", "mpeg",
                    "ogv", "ogg", "wmv", "flv", "m2ts", "3gp", "y4m"}))
    return Kind::Video;
  if (IsOneOf(ext, {"md", "markdown", "mdown", "mkd"}))
    return Kind::Markdown;
  if (IsOneOf(ext, {"html", "htm", "xhtml"}))
    return Kind::Html;
  if (IsOneOf(ext, {"c", "cc", "cpp", "cxx", "h", "hpp", "py", "rb", "pl",
                    "js", "jsx", "tsx", "css", "java", "kt", "go", "rs", "sh",
                    "bash", "fish", "json", "xml", "yml", "yaml", "toml", "lua",
                    "swift", "sql", "cmake", "patch", "diff"}) ||
      IsOneOf(leaf, {"Makefile", "Dockerfile"}) || bytes.rfind("#!", 0) == 0)
    return Kind::Code;
  if (bytes.size() >= 12 && bytes.rfind("RIFF", 0) == 0 && bytes.compare(8, 4, "WAVE") == 0)
    return Kind::Audio;
  if (bytes.rfind("fLaC", 0) == 0 || bytes.rfind("ID3", 0) == 0)
    return Kind::Audio;
  if ((bytes.size() >= 8 && bytes.compare(4, 4, "ftyp") == 0) || bytes.rfind("OggS", 0) == 0)
    return Kind::Video;
  try {
    Utf8Length(bytes, bytes.size() == 1024);
    return ext == "ts" ? Kind::Code : Kind::Text;
  } catch (const std::runtime_error &) {
    return ext == "ts" ? Kind::Video : Kind::Unknown;
  }
}

void ValidateText(std::string_view text) { Utf8Length(text, false); }

std::string ReadText(Input &input, bool &truncated, size_t limit) {
  auto content = ReadPrefix(input, limit + 1);
  truncated = content.size() > limit;
  content.resize(Utf8Length(std::string_view(content).substr(0, limit), truncated));
  if (content.rfind("\xef\xbb\xbf", 0) == 0)
    content.erase(0, 3);
  // Normalize CRLF and classic CR, without creating an extra blank line.
  std::string text;
  text.reserve(content.size());
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] != '\r')
      text.push_back(content[i]);
    else if (i + 1 == content.size() || content[i + 1] != '\n')
      text.push_back('\n');
  }
  return text;
}

std::unique_ptr<BBitmap> RenderPdf(Input &input, int page,
                                    const Listi::TransferControl &control) {
  if (page < 1)
    throw std::runtime_error("Invalid PDF page");
  ScratchPdf files;
  CopyPdf(input, files.Source(), control);
  const std::string pageNumber = std::to_string(page);
  const std::string source = files.Source();
  const std::string prefix = files.Prefix();
  const char *args[] = {"/boot/system/bin/pdftoppm", "-q", "-f", pageNumber.c_str(),
                        "-l", pageNumber.c_str(), "-singlefile", "-png", "-scale-to", "1600",
                        source.c_str(), prefix.c_str(), nullptr};
  pid_t child;
  control.Check();
  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0)
    throw std::runtime_error("Cannot prepare PDF renderer");
  int result = posix_spawn_file_actions_addopen(
      &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  if (result == 0)
    result = posix_spawn(&child, args[0], &actions, nullptr,
                         const_cast<char *const *>(args), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (result != 0)
    throw std::runtime_error("PDF renderer unavailable (pdftoppm)");
  try {
    bigtime_t deadline = system_time() + 8000000;
    int status;
    while (true) {
      pid_t done = waitpid(child, &status, WNOHANG);
      if (done == child) {
        child = -1;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
          throw std::runtime_error("PDF page unavailable or document is locked");
        break;
      }
      if (done < 0 && errno != EINTR) {
        child = -1;
        throw std::runtime_error("Cannot wait for PDF renderer");
      }
      control.Check();
      if (system_time() >= deadline)
        throw std::runtime_error("PDF rendering timed out");
      snooze(20000);
    }
  } catch (...) {
    if (child >= 0) {
      kill(child, SIGKILL);
      while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
    }
    throw;
  }
  if (access(files.Image().c_str(), R_OK) != 0)
    throw std::runtime_error("PDF page unavailable");
  return std::unique_ptr<BBitmap>(LoadPreviewBitmap(files.Image(), control));
}

std::string InspectMedia(Input &input, Kind kind, bool play,
                         const Listi::TransferControl &control,
                         const PresentFrame &present) {
  input.Seek(0, SEEK_SET);
  BMediaFile file(&input);
  if (file.InitCheck() != B_OK)
    throw std::runtime_error("Cannot open media with Haiku's installed codecs");
  BMediaTrack *audio = nullptr, *video = nullptr;
  media_format videoFormat{};
  BString caption;
  for (int32 i = 0; i < file.CountTracks() && i < 128; ++i) {
    control.Check();
    BMediaTrack *track = file.TrackAt(i);
    if (!track)
      continue;
    media_format encoded{};
    if (track->EncodedFormat(&encoded) == B_OK) {
      if (!audio && (encoded.type == B_MEDIA_ENCODED_AUDIO ||
                     encoded.type == B_MEDIA_RAW_AUDIO)) {
        audio = track;
        continue;
      }
      if (!video && kind == Kind::Video &&
          (encoded.type == B_MEDIA_ENCODED_VIDEO || encoded.type == B_MEDIA_RAW_VIDEO)) {
        videoFormat.type = B_MEDIA_RAW_VIDEO;
        videoFormat.u.raw_video.display.format = B_RGB32;
        if (track->DecodedFormat(&videoFormat) == B_OK) {
          const auto &display = videoFormat.u.raw_video.display;
          if (display.format == B_RGB32 && display.line_width &&
              display.line_width <= 4096 && display.line_count &&
              display.line_count <= 2160 && display.bytes_per_row >= display.line_width * 4 &&
              display.bytes_per_row <= 4096 * 4) {
            video = track;
            continue;
          }
        }
      }
    }
    file.ReleaseTrack(track);
  }
  if (!video && !audio)
    throw std::runtime_error("No supported audio or video tracks");
  const bigtime_t duration = std::max(video ? video->Duration() : 0,
                                      audio ? audio->Duration() : 0);
  const auto &display = videoFormat.u.raw_video.display;
  if (video)
    caption << "Video " << display.line_width << " × " << display.line_count;
  else
    caption << "Audio";
  if (duration > 0) {
    auto seconds = duration / 1000000;
    caption << " · " << seconds / 60 << ":" << (seconds % 60 < 10 ? "0" : "")
            << seconds % 60;
  }
  SoundClip clip;
  if (play && audio) {
    const bool output = AudioOutputAvailable();
    if (output)
      clip.Start(*audio, control);
    if (!clip.player) {
      if (!video)
        throw std::runtime_error(output ? "Cannot play this audio track"
                                        : "No system audio output is available");
      caption << " · audio unavailable";
    }
  }
  const bigtime_t start = system_time();
  if (video) {
    const double rate = videoFormat.u.raw_video.field_rate;
    const bigtime_t period = std::isfinite(rate) && rate >= 1 && rate <= 120
                                ? bigtime_t(1000000 / rate) : 33333;
    for (bigtime_t elapsed = 0; elapsed < kClipTime; elapsed += period) {
      control.Check();
      if (elapsed && system_time() >= start + kClipTime)
        break;
      auto frame = ReadVideoFrame(*video, display);
      if (!frame) {
        if (!elapsed)
          throw std::runtime_error("Cannot decode video frames");
        break;
      }
      if (play && elapsed)
        WaitUntil(start + elapsed, control);
      present(std::move(frame));
      if (!play)
        break;
    }
  }
  if (play && clip.player) {
    // The SoundPlayer node starts asynchronously after its output latency.
    const auto startup = std::clamp<bigtime_t>(clip.player->Latency() + 5000, 0, 250000);
    WaitUntil(start + clip.duration + startup, control);
  }
  return caption.String();
}
} // namespace Inspector
