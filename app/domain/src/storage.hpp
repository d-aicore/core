// Opening and saving documents on disk (plan 3.2, "Gravação").
// Saving writes a temporary file next to the target, reads it back, then replaces the target in one
// step and keeps the previous version as "<file>.bak". Any failure leaves the previous file usable.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include "document.hpp"

namespace core::doc {

constexpr const char* kFileExtension = ".core";

struct SaveError : std::runtime_error { using std::runtime_error::runtime_error; };

struct OpenResult {
  Document document;
  // Opened from another format (core-project v2): it must be saved under a new name; the source
  // file is never written.
  bool imported = false;
  std::vector<std::string> notes;  // import report, for the user
};

// Throws DocumentError (invalid), FutureVersionError (newer app: the file is left alone), or
// std::runtime_error (cannot read).
OpenResult openFile(const std::filesystem::path& path);
void saveFile(const Document& document, const std::filesystem::path& path);  // throws SaveError

// The same safe save for any text file (templates...): temporary file, read back, check(text) on what
// is on disk, then replace (the previous file becomes "<file>.bak"). Throws SaveError.
void writeFileSafely(const std::string& text, const std::filesystem::path& path, const std::function<bool(const std::string&)>& check);
std::string readTextFile(const std::filesystem::path& path);  // throws std::runtime_error

std::filesystem::path backupPath(const std::filesystem::path& path);  // "<file>.bak"

// What a file was when this window last read or wrote it, to see at the next Save whether somebody else (another
// window of Core, another program) wrote it meanwhile. Its content decides (the size, then the SHA-256): a file that
// was only touched, or written again with the same content, did not change.
struct FileStamp {
  bool exists = false;
  std::uintmax_t size = 0;
  std::string sha256;  // "" when the file could not be read
};
FileStamp fileStamp(const std::filesystem::path& path);  // never throws
enum class DiskChange {
  Same,     // what is there is what this window knows: Save may replace it
  Changed,  // other content, a file where there was none, or one that cannot be read: Save asks first
  Gone,     // there is no file (any more): Save writes it again, nothing is overwritten
};
DiskChange diskChange(const FileStamp& known, const std::filesystem::path& path);

} // namespace core::doc
