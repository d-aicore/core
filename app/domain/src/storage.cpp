#include "storage.hpp"
#include <atomic>
#include <fstream>
#include <sstream>
#include <system_error>
#include "sha256.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace core::doc {

namespace fs = std::filesystem;

namespace {

// For messages: UTF-8, whatever characters the name has (path::string() throws on Windows for names
// outside the ANSI code page).
std::string show(const fs::path& p) {
  const std::u8string s = p.u8string();
  return std::string(s.begin(), s.end());
}
fs::path withSuffix(const fs::path& p, const std::string& suffix) { return fs::path(p.native() + fs::path(suffix).native()); }

std::string readAll(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot read " + show(path) + ".");
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

#ifdef _WIN32
std::string windowsError(DWORD code) {
  // In English like the rest of the message; a Windows without the English texts answers nothing for that language
  // (it showed "error 1175 (1175)"), so the system's own language is asked next. Read wide: the message is UTF-8.
  wchar_t* text = nullptr;
  DWORD length = 0;
  for (const DWORD language : {(DWORD)MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (DWORD)0}) {
    length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
                            language, (LPWSTR)&text, 0, nullptr);
    if (length > 0 && text) break;
    text = nullptr;
  }
  std::string message;
  if (text) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, (int)length, nullptr, 0, nullptr, nullptr);
    if (bytes > 0) {
      message.resize((size_t)bytes);
      WideCharToMultiByte(CP_UTF8, 0, text, (int)length, message.data(), bytes, nullptr, nullptr);
    }
    LocalFree(text);
  }
  if (message.empty()) message = "error " + std::to_string(code);
  while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == '.')) message.pop_back();
  return message + " (" + std::to_string(code) + ")";
}
#endif

// The bytes are on the disk, not only in a cache, before the file replaces the previous one.
void flushToDisk(const fs::path& path) {
#ifdef _WIN32
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) throw SaveError("Cannot reopen " + show(path) + ": " + windowsError(GetLastError()));
  const BOOL ok = FlushFileBuffers(h);
  const DWORD code = GetLastError();
  CloseHandle(h);
  if (!ok) throw SaveError("Cannot flush " + show(path) + ": " + windowsError(code));
#else
  (void)path;
#endif
}

// The system's steps, each returning 0 or the system's error code.
// One step: the target gets the new content and the old one becomes the backup (an older backup is replaced only
// when this succeeds).
unsigned long systemReplace(const fs::path& target, const fs::path& temporary, const fs::path& backup) {
#ifdef _WIN32
  if (ReplaceFileW(target.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) return 0;
  const DWORD code = GetLastError();
  return code ? code : ERROR_GEN_FAILURE;
#else
  std::error_code e;
  fs::copy_file(target, backup, fs::copy_options::overwrite_existing, e);
  e.clear();
  fs::rename(temporary, target, e);
  return e ? (unsigned long)e.value() : 0;
#endif
}

unsigned long systemMove(const fs::path& from, const fs::path& to) {
#ifdef _WIN32
  if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH)) return 0;
  const DWORD code = GetLastError();
  return code ? code : ERROR_GEN_FAILURE;
#else
  std::error_code e;
  fs::rename(from, to, e);
  return e ? (unsigned long)e.value() : 0;
#endif
}

std::string systemError(unsigned long code) {
#ifdef _WIN32
  return windowsError((DWORD)code);
#else
  return std::generic_category().message((int)code) + " (" + std::to_string(code) + ")";
#endif
}

ReplaceStep replaceStep = systemReplace;
WriteFault writeFault = nullptr;

// Puts the written temporary file in the target's place. When it throws with previousFileIntact, the target is as
// it was and the temporary file may go; otherwise the temporary file is the only copy of the new version and stays.
void replaceWith(const fs::path& temporary, const fs::path& target) {
  if (!fs::exists(target)) {
    if (const unsigned long code = systemMove(temporary, target))
      throw SaveError("Cannot create " + show(target) + ": " + systemError(code));
    return;
  }
  const fs::path backup = backupPath(target);
  const unsigned long code = replaceStep(target, temporary, backup);
  if (code == 0) return;
  // A failure is not always "nothing happened" (2026-11-18, D4): ERROR_UNABLE_TO_MOVE_REPLACEMENT_2 (1177) leaves the
  // old file already renamed to the backup and the new one still under its temporary name. What is on disk decides.
  std::error_code e;
  if (fs::is_regular_file(target, e)) throw SaveError("Cannot replace " + show(target) + ": " + systemError(code));
  if (fs::is_regular_file(temporary, e) && systemMove(temporary, target) == 0) return;  // finished here: new in place, previous in the backup
  const bool hasNew = fs::is_regular_file(temporary, e), hasPrevious = fs::is_regular_file(backup, e);
  std::string where = hasNew ? "the new version is in " + show(temporary) : std::string();
  if (hasPrevious) where += (hasNew ? " and " : "") + ("the previous version is in " + show(backup));
  SaveError error("Cannot replace " + show(target) + ": " + systemError(code) + ". The file is not in its place" +
                  (where.empty() ? std::string(".")
                                 : ": " + where + ". Nothing was deleted: give " + (hasNew && hasPrevious ? "one of them" : "it") +
                                       " the name " + show(target.filename()) + " to open it."));
  error.previousFileIntact = false;
  throw error;
}

} // namespace

void setReplaceStepForTests(ReplaceStep step) { replaceStep = step ? step : systemReplace; }

void setWriteFaultForTests(WriteFault fault) { writeFault = fault; }
void writeFaultForTests(std::ostream& out, const fs::path& file) {
  if (writeFault) writeFault(out, file);
}

fs::path backupPath(const fs::path& path) { return withSuffix(path, ".bak"); }

OpenResult openFile(const fs::path& path) {
  const std::string text = readAll(path);
  const json value = json::parse(text, nullptr, false);
  if (value.is_discarded() || !value.is_object()) throw DocumentError("", "the file is not a Core document (not JSON)");
  OpenResult result;
  const std::string format = value.value("format", "");
  if (format == kDocumentFormat) {
    int migratedFrom = 0;
    result.document = parseDocument(value, &migratedFrom);
    if (migratedFrom) {  // never overwrite the old file: the migrated song is saved under a new name
      result.imported = true;
      result.notes.push_back("converted from schema " + std::to_string(migratedFrom) + ": each track's pattern is now pattern 1, placed over the song");
    }
  } else if (format == "core-project") {
    ImportReport report;
    result.document = importProjectV2(value, &report);
    result.imported = true;
    result.notes = report.converted;
    result.notes.insert(result.notes.end(), report.kept.begin(), report.kept.end());
  } else {
    throw DocumentError("format", "is not a Core document");
  }
  return result;
}

void writeFileSafely(const std::string& text, const fs::path& path, const std::function<bool(const std::string&)>& check) {
  static std::atomic<int> counter{0};
  // Same folder, so the final step is a rename on the same volume.
  const fs::path temporary = withSuffix(path, ".saving-" + std::to_string(counter++));
  auto discard = [&] { std::error_code ignored; fs::remove(temporary, ignored); };
  try {
    {
      std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
      if (!out) throw SaveError("Cannot write in " + show(path.parent_path()) + ".");
      out << text;
      writeFaultForTests(out, temporary);  // test-only seam: no fault is set outside the tests
      out.close();
      if (!out) throw SaveError("Writing " + show(temporary) + " failed (disk full?).");
    }
    flushToDisk(temporary);
    // Read back what is on disk: only a complete, valid file may replace the previous one.
    const std::string back = readAll(temporary);
    if (back != text || !check(back)) throw SaveError("The saved copy does not match the document.");
    replaceWith(temporary, path);
  } catch (const SaveError& e) {
    if (e.previousFileIntact) discard();  // (otherwise the temporary file is the new version's only copy)
    throw;
  } catch (const std::exception& e) {
    discard();
    throw SaveError(e.what());
  }
}

void saveFile(const Document& document, const fs::path& path) {
  writeFileSafely(toJson(document).dump(2) + "\n", path,
                  [&](const std::string& text) { return sameContent(parseDocument(json::parse(text)), document); });
}

std::string readTextFile(const fs::path& path) { return readAll(path); }

FileStamp fileStamp(const fs::path& path) {
  FileStamp stamp;
  std::error_code e;
  if (path.empty() || !fs::exists(path, e) || e) return stamp;
  stamp.exists = true;
  stamp.size = fs::file_size(path, e);
  if (e) stamp.size = 0;
  try { stamp.sha256 = sha256File(path); } catch (const std::exception&) { stamp.sha256.clear(); }
  return stamp;
}

DiskChange diskChange(const FileStamp& known, const fs::path& path) {
  const FileStamp now = fileStamp(path);
  if (!now.exists) return DiskChange::Gone;
  if (!known.exists) return DiskChange::Changed;  // somebody made a file where this window knew none
  if (known.sha256.empty() || now.sha256.empty()) return DiskChange::Changed;  // not read: not known to be the same
  return now.size == known.size && now.sha256 == known.sha256 ? DiskChange::Same : DiskChange::Changed;
}

} // namespace core::doc
