#include "autosave.hpp"
#include <algorithm>
#include <cctype>
#include "storage.hpp"

namespace core::doc {

namespace fs = std::filesystem;

namespace {
// {format, version, session, process, songPath, name, savedAt, document}
constexpr const char* kFormat = "core-autosave";
}  // namespace

fs::path autosavePath(const fs::path& folder, const std::string& session) { return folder / fs::u8path(session + kAutosaveExtension); }

void writeAutosave(const fs::path& folder, const std::string& session, int64_t process, const Document& document,
                   const std::string& songPath, int64_t nowMs) {
  std::error_code e;
  fs::create_directories(folder, e);
  if (e) throw SaveError("Cannot create the autosave folder: " + e.message());
  json doc = toJson(document);
  // Only what the reader takes back is kept: an autosave that recovery then refuses is no autosave (audit 2026-11-07).
  try { (void)parseDocument(doc); } catch (const std::exception& e) { throw SaveError(std::string("The song cannot be read back: ") + e.what()); }
  const json saved = {{"format", kFormat}, {"version", 1}, {"session", session}, {"process", process}, {"songPath", songPath},
                      {"name", document.name}, {"savedAt", nowMs}, {"document", std::move(doc)}};
  const std::string text = saved.dump();
  writeFileSafely(text, autosavePath(folder, session), [&](const std::string& written) { return written == text; });
}

std::vector<AutosaveInfo> findAutosaves(const fs::path& folder, const std::string& exceptSession) {
  std::vector<AutosaveInfo> out;
  std::error_code e;
  if (folder.empty() || !fs::is_directory(folder, e)) return out;
  for (const auto& entry : fs::directory_iterator(folder, e)) {
    if (!entry.is_regular_file(e) || entry.path().extension() != kAutosaveExtension) continue;
    try {
      const json j = json::parse(readTextFile(entry.path()));
      if (j.value("format", std::string()) != kFormat || !j.contains("document")) continue;
      AutosaveInfo info;
      info.file = entry.path();
      info.session = j.value("session", std::string());
      if (info.session == exceptSession) continue;
      info.process = j.value("process", (int64_t)0);
      info.songPath = j.value("songPath", std::string());
      info.name = j.value("name", std::string());
      info.savedAtMs = j.value("savedAt", (int64_t)0);
      out.push_back(std::move(info));
    } catch (const std::exception&) {  // half written, or not ours: left alone
    }
  }
  std::sort(out.begin(), out.end(), [](const AutosaveInfo& a, const AutosaveInfo& b) { return a.savedAtMs > b.savedAtMs; });
  return out;
}

Document readAutosave(const fs::path& file) {
  // Audit 2 (2026-11-16): a damaged autosave says so in plain words (the reader shows it and leaves the file).
  const json j = json::parse(readTextFile(file), nullptr, false);
  if (j.is_discarded()) throw std::runtime_error("The autosave is damaged (not readable JSON).");
  if (!j.is_object() || !j.contains("format") || j["format"] != kFormat || !j.contains("document"))
    throw std::runtime_error("This is not an autosave of Core.");
  return parseDocument(j["document"]);
}

void removeAutosave(const fs::path& file) {
  std::error_code e;
  fs::remove(file, e);
  fs::remove(backupPath(file), e);
}

namespace {
// "C:\Program Files\Core\Core.exe" -> "core.exe" (either separator; ASCII case, as Windows names compare).
std::string imageName(const std::string& image) {
  const size_t slash = image.find_last_of("/\\");
  std::string name = slash == std::string::npos ? image : image.substr(slash + 1);
  for (auto& c : name) c = (char)std::tolower((unsigned char)c);
  return name;
}
}  // namespace

// A live process with that id is not enough: Windows gives the id of a Core that crashed to the next program that
// starts, and its autosave was then never offered (2026-11-18, D2). The writer is still running only when that
// process is Core itself and was already there when the autosave was written. In doubt (no name, no start time, no
// date in the autosave) it counts as gone: an autosave offered once too often loses nothing.
bool autosaveWriterRunning(int64_t process, int64_t savedAtMs, const std::string& ownImage,
                           const std::function<ProcessProbe(int64_t)>& probe) {
  if (process <= 0 || !probe) return false;
  const ProcessProbe p = probe(process);
  if (!p.exists) return false;
  const std::string own = imageName(ownImage);
  if (own.empty() || imageName(p.image) != own) return false;
  return p.createdAtMs > 0 && p.createdAtMs <= savedAtMs;
}

bool AutosaveTracker::finished(bool ok, const std::string& error) {
  writing_ = false;
  if (ok) {
    written_ = std::move(pending_);
    pending_.clear();
    const bool recovered = failures_ > 0;
    failures_ = 0;
    lastError_.clear();
    return recovered;
  }
  pending_.clear();  // not on disk: the same text is due again
  lastError_ = error.empty() ? std::string("unknown error") : error;
  return ++failures_ == 1;
}

} // namespace core::doc
