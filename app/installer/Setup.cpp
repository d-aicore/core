// Core Setup.exe (M6): installs Core for the current user only (no administrator rights).
//   Core Setup.exe                                   a window: install (and copy the sample library), update, go back, uninstall
//   Core Setup.exe --install <folder> [--no-shortcuts] [--samples <library> | --no-samples]   silent (tests); exit 0 when installed
//   Core Setup.exe --uninstall <folder> [--no-shortcuts] [--samples <library>]  silent
// Layout of <folder> (default %LOCALAPPDATA%\Programs\Core):
//   app-<version>\  Core.exe, core-engine.exe, demo, lang, patterns, pcore (from the package inside this program)
//   current.txt     the version the shortcuts start; previous.txt: the one before (kept for going back)
//   Uninstall.exe   a copy of this program
//   Samples\        the sample library, part of Core (copied, not packed: ~7 GB does not fit in an .exe). It sits
//                   beside the versions so an update copies only the files that changed; Core's Library uses it.
//   samples.txt     the files of Samples\ this installer copied, each with the sha256 of the content it
//                   installed ('<path>\t<hash>'; a plain path is a legacy entry). Uninstalling removes only
//                   those whose content still proves they are Core's copy; user-edited/user-owned files stay.
// The library is copied from --samples <folder>, else a Samples folder beside this program, else D:\Samples
// (Diogo's PC). The source is only read; its _core-plan folder (scripts and notes) is not copied.
// Songs, settings, plugins lists, autosaves and logs are never touched: they live in the user's folders.
#include <cstdio>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "CoreSetupData.h"

namespace {

const juce::String kVersion = JUCE_APPLICATION_VERSION_STRING;
const juce::String kUninstallKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Core\\";

juce::File defaultFolder() {
  return juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Programs").getChildFile("Core");
}
juce::File startMenuLink() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("Microsoft\\Windows\\Start Menu\\Programs\\Core.lnk");
}
juce::File desktopLink() { return juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("Core.lnk"); }

juce::String installedVersion(const juce::File& folder) { return folder.getChildFile("current.txt").loadFileAsString().trim(); }
juce::File appFolder(const juce::File& folder, const juce::String& version) { return folder.getChildFile("app-" + version); }

// Points the shortcuts and the Windows "Installed apps" entry at a version that is in the folder.
void integrate(const juce::File& folder, const juce::String& version, bool desktop) {
  const juce::File exe = appFolder(folder, version).getChildFile("Core.exe");
  startMenuLink().getParentDirectory().createDirectory();
  exe.createShortcut("Core", startMenuLink());
  if (desktop || desktopLink().existsAsFile()) exe.createShortcut("Core", desktopLink());
  juce::WindowsRegistry::setValue(kUninstallKey + "DisplayName", "Core");
  juce::WindowsRegistry::setValue(kUninstallKey + "DisplayVersion", version);
  juce::WindowsRegistry::setValue(kUninstallKey + "Publisher", "Core");
  juce::WindowsRegistry::setValue(kUninstallKey + "InstallLocation", folder.getFullPathName());
  juce::WindowsRegistry::setValue(kUninstallKey + "DisplayIcon", exe.getFullPathName());
  juce::WindowsRegistry::setValue(kUninstallKey + "UninstallString", "\"" + folder.getChildFile("Uninstall.exe").getFullPathName() + "\" --uninstall");
  juce::WindowsRegistry::setValue(kUninstallKey + "NoModify", (juce::uint32)1);
  juce::WindowsRegistry::setValue(kUninstallKey + "NoRepair", (juce::uint32)1);
}

// ---------------------------------------------------------------- the sample library

juce::File samplesSource(const juce::String& given) {
  if (given.isNotEmpty()) return juce::File(given);
  const juce::File beside = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("Samples");
  if (beside.isDirectory()) return beside;
  const juce::File usual("D:\\Samples");
  return usual.isDirectory() ? usual : juce::File();
}

bool skipped(const juce::String& relative) { return relative.startsWith("_core-plan"); }

// SHA-256 (FIPS 180-4), self-contained: the installer links only juce_gui_basics (no juce_cryptography,
// no core_domain), and ownership of a sample is proven by its content, never by its name, size or date.
// The same implementation as app/domain/src/sha256.cpp (proven by domain_tests).
class Sha256 {
public:
  void update(const unsigned char* p, size_t n) {
    total_ += n;
    while (n > 0) {
      const size_t take = std::min(n, sizeof(block_) - fill_);
      std::memcpy(block_ + fill_, p, take);
      fill_ += take; p += take; n -= take;
      if (fill_ == sizeof(block_)) { compress(); fill_ = 0; }
    }
  }
  juce::String hex() {
    const juce::uint64 bits = total_ * 8;
    const unsigned char one = 0x80, zero = 0;
    update(&one, 1);
    while (fill_ != 56) update(&zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8 * i));
    update(len, 8);
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (juce::uint32 v : h_)
      for (int i = 28; i >= 0; i -= 4) out += digits[(v >> i) & 15];
    return juce::String(out);
  }

private:
  void compress() {
    juce::uint32 w[64];
    for (int i = 0; i < 16; ++i) w[i] = (juce::uint32)block_[4 * i] << 24 | (juce::uint32)block_[4 * i + 1] << 16 | (juce::uint32)block_[4 * i + 2] << 8 | block_[4 * i + 3];
    for (int i = 16; i < 64; ++i) {
      const juce::uint32 s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const juce::uint32 s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    juce::uint32 a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
    for (int i = 0; i < 64; ++i) {
      const juce::uint32 t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[i] + w[i];
      const juce::uint32 t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
  }
  static juce::uint32 rotr(juce::uint32 x, int n) { return (x >> n) | (x << (32 - n)); }
  static constexpr juce::uint32 kK[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
      0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
      0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
      0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
      0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
      0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  juce::uint32 h_[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  unsigned char block_[64] = {};
  size_t fill_ = 0;
  juce::uint64 total_ = 0;
};

// The sha256 of a file's bytes; empty when it cannot be read (ownership then cannot be proven).
juce::String sha256OfFile(const juce::File& file) {
  juce::FileInputStream in(file);
  if (!in.openedOk()) return {};
  Sha256 sha;
  std::vector<char> buffer(1 << 20);
  while (!in.isExhausted()) {
    const int n = in.read(buffer.data(), (int)buffer.size());
    if (n <= 0) break;
    sha.update((const unsigned char*)buffer.data(), (size_t)n);
  }
  return sha.hex();
}

// samples.txt: one entry per line, '<relative path>\t<sha256 of the content Core installed>'. A plain
// path without the tab is a LEGACY entry (an older installer recorded no hash): its ownership is proven
// against the distributed source instead, and never assumed.
struct SampleEntry { juce::String rel, hash; };  // hash empty = legacy

juce::String sampleEntryLine(const SampleEntry& e) { return e.hash.isEmpty() ? e.rel : e.rel + "\t" + e.hash; }

std::vector<SampleEntry> readSampleEntries(const juce::File& folder) {
  std::vector<SampleEntry> entries;
  for (const auto& line : juce::StringArray::fromLines(folder.getChildFile("samples.txt").loadFileAsString())) {
    if (line.isEmpty()) continue;
    const int tab = line.indexOfChar('\t');
    if (tab < 0) entries.push_back({line, juce::String()});
    else entries.push_back({line.substring(0, tab), line.substring(tab + 1)});
  }
  return entries;
}

// Copies the library into <folder>\Samples. Ownership is proven by CONTENT (sha256), never by size+date:
// a file the user edited or placed at a distributed path is kept — never overwritten, and dropped from
// the manifest so uninstall never deletes it. Core's own copies (hash matches what Core installed; legacy
// entries: hash matches the distributed source) are updated when the source differs. A pre-existing file
// that was never listed is left alone (Audit 27), even when it matches path+size+date.
// progress(done, total) is called from this thread.
juce::String copySamples(const juce::File& source, const juce::File& folder, const std::function<void(int, int)>& progress) {
  if (source == juce::File()) return {};
  if (!source.isDirectory()) return "The sample library is not there: " + source.getFullPathName();
  const juce::File target = folder.getChildFile("Samples");
  if (target == source) return {};
  juce::Array<juce::File> files;
  for (const auto& entry : juce::RangedDirectoryIterator(source, true, "*", juce::File::findFiles))
    if (!skipped(entry.getFile().getRelativePathFrom(source).replaceCharacter('\\', '/'))) files.add(entry.getFile());
  // Audit 27: the manifest read BEFORE copying decides ownership. A path becomes ours only when this
  // install really copied it, or an earlier install listed it AND the content still proves it is the
  // copy Core installed. Before, a pre-existing file that merely matched path+size+date (copied back
  // from a backup, robocopy /COPY:DAT) was listed without ever being copied, and uninstall deleted the
  // user's file; size+date alone also hid user content of the same length (S5).
  const std::vector<SampleEntry> earlier = readSampleEntries(folder);
  juce::StringArray owned, processed;
  int failed = 0;
  for (int i = 0; i < files.size(); ++i) {
    const juce::File& from = files.getReference(i);
    const juce::String relative = from.getRelativePathFrom(source);
    const juce::File to = target.getChildFile(relative);
    const juce::String rel = relative.replaceCharacter('\\', '/');
    processed.add(rel);
    const SampleEntry* old = nullptr;
    for (const auto& e : earlier) if (e.rel == rel) { old = &e; break; }
    if (!to.existsAsFile()) {
      const juce::String hash = sha256OfFile(from);
      if (hash.isNotEmpty() && to.getParentDirectory().createDirectory() && from.copyFileTo(to)) owned.add(sampleEntryLine({rel, hash}));
      else ++failed;
    } else if (old == nullptr) {
      // Never listed: not proven ours, so it is the user's — leave it, never claim it (Audit 27).
    } else {
      const juce::String srcHash = sha256OfFile(from);
      const juce::String dstHash = sha256OfFile(to);
      const bool ours = old->hash.isNotEmpty() ? dstHash == old->hash
                                               : dstHash.isNotEmpty() && dstHash == srcHash;
      if (ours && dstHash == srcHash) {
        owned.add(sampleEntryLine({rel, srcHash}));  // intact Core copy (a legacy entry is upgraded to hashed)
      } else if (ours) {
        // Core's own outdated copy: a legitimate update replaces it and records the new content.
        if (from.copyFileTo(to)) owned.add(sampleEntryLine({rel, srcHash}));
        else ++failed;
      } else {
        // User-edited/user-owned content at a distributed path: keep it, and drop the entry so
        // uninstall never deletes it.
        std::fputs(("Kept your file (not Core's installed copy): " + to.getFullPathName() + "\n").toRawUTF8(), stdout);
      }
    }
    if (progress && (i % 25 == 0 || i + 1 == files.size())) progress(i + 1, files.size());
  }
  // Earlier installs' files that this library no longer carries stay listed too (still ours to remove,
  // re-validated by content at uninstall).
  for (const auto& e : earlier)
    if (!processed.contains(e.rel)) owned.add(sampleEntryLine(e));
  owned.removeDuplicates(true);
  folder.getChildFile("samples.txt").replaceWithText(owned.joinIntoString("\n") + "\n");
  if (failed > 0) return juce::String(failed) + " samples could not be copied to " + target.getFullPathName() + " (is the disk full?). Install again to finish.";
  return {};
}

// Removes the samples this installer copied, then the folders left empty. Anything else in Samples\ stays.
// Ownership is re-validated by CONTENT at removal: a listed file is deleted only when its sha256 still
// matches what Core installed. A legacy plain-path entry is deleted only when it matches the distributed
// source AND that source is available at uninstall — otherwise ownership cannot be proven and the file is
// kept (conservative: data first). User-edited/user-owned files are kept and dropped from the manifest.
bool removeSamples(const juce::File& folder, const juce::File& source) {
  const juce::File target = folder.getChildFile("Samples"), list = folder.getChildFile("samples.txt");
  // Running Uninstall.exe from the install folder would put its own Samples beside it: comparing a file
  // against itself proves nothing, so that "source" counts as unavailable.
  const bool haveSource = source != juce::File() && source.isDirectory() && source != target;
  bool ok = true;
  juce::StringArray remaining;
  for (const auto& e : readSampleEntries(folder)) {
    if (e.rel.isEmpty() || e.rel.contains("..")) continue;
    const juce::File f = target.getChildFile(e.rel);
    if (!f.existsAsFile()) continue;  // already gone: the entry is dropped
    const juce::String current = sha256OfFile(f);
    bool proven = false;
    if (e.hash.isNotEmpty()) {
      proven = current == e.hash;
    } else if (haveSource) {
      const juce::File src = source.getChildFile(e.rel);
      if (src.existsAsFile()) proven = current.isNotEmpty() && current == sha256OfFile(src);
    }
    if (proven) {
      if (f.deleteFile()) continue;
      ok = false;
      remaining.add(sampleEntryLine(e));  // in use: stays listed for a retry
    } else {
      std::fputs(("Kept your file at uninstall (not proven to be Core's copy): " + f.getFullPathName() + "\n").toRawUTF8(), stdout);
    }
  }
  if (target.isDirectory()) {
    juce::Array<juce::File> dirs = target.findChildFiles(juce::File::findDirectories, true);
    for (int i = dirs.size(); --i >= 0;)  // deepest first
      if (dirs[i].getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0) dirs[i].deleteFile();
    if (target.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0) target.deleteFile();
  }
  if (ok) list.deleteFile();
  else list.replaceWithText(remaining.joinIntoString("\n") + "\n");
  return ok;
}

// A version owns only the files listed by its package. Unknown files in an app-* folder
// may be songs/samples saved there by the user, so never recursively delete that folder.
constexpr const char* kAppManifest = ".core-installed-files.txt";
juce::StringArray packageFiles() {
  juce::MemoryInputStream package(CoreSetupData::CorePackage_zip, CoreSetupData::CorePackage_zipSize, false);
  juce::ZipFile zip(package);
  juce::StringArray files;
  for (int i = 0; i < zip.getNumEntries(); ++i) {
    const auto name = zip.getEntry(i)->filename.replaceCharacter('\\', '/');
    if (!name.endsWithChar('/')) files.add(name);
  }
  return files;
}
juce::StringArray installedAppFiles(const juce::File& folder) {
  const auto manifest = folder.getChildFile(kAppManifest);
  // An older installer left no manifest: only its fixed executable names are certain.
  // Do not use this version's ZIP to guess ownership of an older version's resources.
  auto files = manifest.existsAsFile() ? juce::StringArray::fromLines(manifest.loadFileAsString())
                                      : juce::StringArray{"Core.exe", "core-engine.exe", "core-clips.exe"};
  files.removeEmptyStrings();
  files.addIfNotAlreadyThere(kAppManifest);
  return files;
}
bool hasAdditionalAppFiles(const juce::File& folder) {
  const auto known = installedAppFiles(folder);
  for (const auto& f : folder.findChildFiles(juce::File::findFiles, true))
    if (!known.contains(f.getRelativePathFrom(folder).replaceCharacter('\\', '/'), true)) return true;
  return false;
}
bool removeInstalledAppFiles(const juce::File& folder) {
  bool ok = true;
  for (const auto& relative : installedAppFiles(folder)) {
    if (juce::File::isAbsolutePath(relative) || relative.contains("..")) continue;
    const auto file = folder.getChildFile(relative);
    if (file.existsAsFile()) ok = file.deleteFile() && ok;
  }
  auto dirs = folder.findChildFiles(juce::File::findDirectories, true);
  std::sort(dirs.begin(), dirs.end(), [](const juce::File& a, const juce::File& b) {
    return a.getFullPathName().length() > b.getFullPathName().length();
  });
  for (const auto& dir : dirs) if (dir.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0) dir.deleteFile();
  if (folder.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0) folder.deleteFile();
  return ok;
}

// Unpacks this program's package into app-<version> (a .part folder first, renamed when complete), makes
// it the current version and keeps the one before it for going back. Older versions are removed.
juce::String install(const juce::File& folder, bool withShortcuts, bool desktop) {
  if (!folder.createDirectory()) return "Cannot create " + folder.getFullPathName();
  juce::MemoryInputStream package(CoreSetupData::CorePackage_zip, CoreSetupData::CorePackage_zipSize, false);
  juce::ZipFile zip(package);
  if (zip.getNumEntries() == 0) return "The package inside the installer is empty.";
  const juce::File target = appFolder(folder, kVersion), part = folder.getChildFile("app-" + kVersion + ".part");
  if (target.isDirectory() && hasAdditionalAppFiles(target))
    return "Additional files are kept in " + target.getFullPathName() + ". Move them out before reinstalling this version.";
  part.deleteRecursively();
  if (auto r = zip.uncompressTo(part); r.failed()) { part.deleteRecursively(); return "Cannot unpack: " + r.getErrorMessage(); }
  if (!part.getChildFile("Core.exe").existsAsFile()) { part.deleteRecursively(); return "The package has no Core.exe."; }
  if (!part.getChildFile(kAppManifest).replaceWithText(packageFiles().joinIntoString("\n") + "\n"))
    return "Cannot write the installed file list.";
  const juce::String before = installedVersion(folder);
  if (target.exists() && !target.deleteRecursively()) {
    part.deleteRecursively();
    return "Core " + kVersion + " is open: close it and try again.";
  }
  if (!part.moveFileTo(target)) return "Cannot move the new version into place.";
  folder.getChildFile("current.txt").replaceWithText(kVersion);
  if (before.isNotEmpty() && before != kVersion) folder.getChildFile("previous.txt").replaceWithText(before);
  // Only the current version and the one before it stay.
  const juce::String keep = folder.getChildFile("previous.txt").loadFileAsString().trim();
  for (const auto& old : folder.findChildFiles(juce::File::findDirectories, false, "app-*"))
    if (old != target && old.getFileName() != "app-" + keep) removeInstalledAppFiles(old);
  const juce::File self = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
  if (self != folder.getChildFile("Uninstall.exe")) self.copyFileTo(folder.getChildFile("Uninstall.exe"));
  if (withShortcuts) integrate(folder, kVersion, desktop);
  return {};
}

// Back to the version kept from before (the shortcuts start it again); the newer one stays, unused.
juce::String goBack(const juce::File& folder) {
  const juce::String previous = folder.getChildFile("previous.txt").loadFileAsString().trim();
  const juce::String current = installedVersion(folder);
  if (previous.isEmpty() || !appFolder(folder, previous).getChildFile("Core.exe").existsAsFile()) return "There is no earlier version to go back to.";
  folder.getChildFile("current.txt").replaceWithText(previous);
  folder.getChildFile("previous.txt").replaceWithText(current);
  integrate(folder, previous, false);
  return {};
}

// A folder Core Setup installed: current.txt names a version whose app-<version>\Core.exe is there.
bool isCoreFolder(const juce::File& folder) {
  const juce::String version = installedVersion(folder);
  return version.isNotEmpty() && appFolder(folder, version).getChildFile("Core.exe").existsAsFile();
}

// What this installer puts in a folder; uninstalling removes only these, never anything else.
bool isOurs(const juce::File& f) {
  const juce::String name = f.getFileName();
  if (f.isDirectory()) return name.startsWith("app-");
  return name == "current.txt" || name == "previous.txt" || name == "Uninstall.exe" || name == "samples.txt";
}

// Removes the program, its shortcuts and its entry. The songs and the user's data stay. A folder that is
// not a Core install is refused, and in one that is, only what the installer put there is removed (the
// folder itself goes only when nothing else is left in it).
juce::String uninstall(const juce::File& folder, bool withShortcuts, const juce::File& samplesLib) {
  if (!isCoreFolder(folder)) return "Not a Core install folder (no current.txt with its app folder): " + folder.getFullPathName();
  if (withShortcuts) {
    startMenuLink().deleteFile();
    desktopLink().deleteFile();
    juce::WindowsRegistry::deleteKey(kUninstallKey.dropLastCharacters(1));
  }
  const juce::File self = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
  bool ok = removeSamples(folder, samplesLib), othersLeft = false;
  for (const auto& f : folder.findChildFiles(juce::File::findFilesAndDirectories, false)) {
    if (!isOurs(f)) { othersLeft = true; continue; }
    if (f.isDirectory()) {
      ok = removeInstalledAppFiles(f) && ok;
      othersLeft = f.exists() || othersLeft;
      continue;
    }
    if (f != self) ok = f.deleteRecursively() && ok;
  }
  if (!ok) return "Some files are in use (is Core open?): close it and try again.";
  if (self.getParentDirectory() == folder) {
    // This program is running from the folder: it is removed by a separate step once it has closed.
    juce::ChildProcess().start("cmd.exe /c ping 127.0.0.1 -n 3 > nul & del /q \"" + self.getFullPathName() + "\" & rmdir \"" + folder.getFullPathName() + "\"", 0);
  } else if (!othersLeft) {
    folder.deleteFile();  // empty by now: deleteFile removes an empty directory
  }
  return {};
}

// ---------------------------------------------------------------- the window

class SetupComponent final : public juce::Component {
public:
  SetupComponent() {
    for (auto* c : std::initializer_list<juce::Component*>{&title_, &text_, &desktop_, &install_, &back_, &remove_, &open_}) addAndMakeVisible(c);
    title_.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    text_.setJustificationType(juce::Justification::topLeft);
    desktop_.setButtonText("Shortcut on the desktop");
    desktop_.setToggleState(true, juce::dontSendNotification);
    install_.onClick = [this] {
      const juce::String problem = install(folder_, true, desktop_.getToggleState());
      if (problem.isNotEmpty()) return report(problem, {});
      const juce::File source = samplesSource({});
      if (source == juce::File()) return report({}, "Core " + kVersion + " is installed (no sample library found to copy).");
      // The library is large (the first copy takes minutes): it is copied on its own thread, with the count shown.
      for (auto* b : {&install_, &back_, &remove_, &open_}) b->setEnabled(false);
      juce::Component::SafePointer<SetupComponent> self(this);
      const juce::File folder = folder_;
      juce::Thread::launch([self, source, folder] {
        const juce::String copyProblem = copySamples(source, folder, [self](int done, int total) {
          juce::MessageManager::callAsync([self, done, total] {
            if (self != nullptr) self->title_.setText("Copying samples... " + juce::String(done) + " / " + juce::String(total), juce::dontSendNotification);
          });
        });
        juce::MessageManager::callAsync([self, copyProblem] {
          if (self != nullptr) self->report(copyProblem, "Core " + kVersion + " is installed, with its sample library.");
        });
      });
    };
    back_.onClick = [this] { report(goBack(folder_), "Core " + installedVersion(folder_) + " is back."); };
    remove_.onClick = [this] {
      juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Uninstall Core")
                                       .withMessage("Remove Core from this PC? Your songs, settings and plugins stay where they are.")
                                       .withButton("Uninstall", 1).withButton("Cancel", 0),
                                   [this](int r) {
                                     if (r != 1) return;
                                     const juce::String problem = uninstall(folder_, true, samplesSource({}));
                                     if (problem.isNotEmpty()) return report(problem, {});
                                     juce::JUCEApplication::quit();
                                   });
    };
    open_.setButtonText("Open Core");
    open_.onClick = [this] {
      appFolder(folder_, installedVersion(folder_)).getChildFile("Core.exe").startAsProcess();
      juce::JUCEApplication::quit();
    };
    refresh();
    setSize(520, 300);
  }

  void resized() override {
    auto r = getLocalBounds().reduced(20);
    title_.setBounds(r.removeFromTop(34));
    auto buttons = r.removeFromBottom(32);
    install_.setBounds(buttons.removeFromLeft(150).reduced(2, 0));
    back_.setBounds(buttons.removeFromLeft(150).reduced(2, 0));
    open_.setBounds(buttons.removeFromRight(100).reduced(2, 0));
    remove_.setBounds(buttons.removeFromRight(100).reduced(2, 0));
    r.removeFromBottom(8);
    desktop_.setBounds(r.removeFromBottom(26));
    text_.setBounds(r);
  }

  void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff15171c)); }

private:
  void refresh() {
    const juce::String installed = installedVersion(folder_);
    const juce::String previous = folder_.getChildFile("previous.txt").loadFileAsString().trim();
    title_.setText(installed.isEmpty() ? "Install Core " + kVersion : "Core " + installed + " is installed", juce::dontSendNotification);
    text_.setText("For this user only, no administrator rights needed.\nFolder: " + folder_.getFullPathName() +
                      "\nA shortcut goes in the Start menu.\n\nYour songs, settings, plugin list, autosaves and logs are never changed by "
                      "installing, updating or uninstalling." + (installed.isNotEmpty() && installed != kVersion ? "\n\nThis installer has Core " + kVersion + "." : juce::String()),
                  juce::dontSendNotification);
    install_.setButtonText(installed.isEmpty() ? "Install" : installed == kVersion ? "Install again" : "Update to " + kVersion);
    back_.setButtonText(previous.isNotEmpty() ? "Go back to " + previous : "Go back");
    back_.setEnabled(installed.isNotEmpty() && previous.isNotEmpty() && appFolder(folder_, previous).exists());
    remove_.setEnabled(installed.isNotEmpty());
    open_.setEnabled(installed.isNotEmpty());
    desktop_.setVisible(installed.isEmpty());
  }

  void report(const juce::String& problem, const juce::String& done) {
    refresh();
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(problem.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon)
                                     .withTitle("Core Setup").withMessage(problem.isEmpty() ? done : problem).withButton("OK", 0), nullptr);
  }

  juce::File folder_ = defaultFolder();
  juce::Label title_, text_;
  juce::ToggleButton desktop_;
  juce::TextButton install_, back_, remove_{"Uninstall"}, open_;
};

class SetupWindow final : public juce::DocumentWindow {
public:
  SetupWindow() : DocumentWindow("Core Setup", juce::Colour(0xff15171c), DocumentWindow::closeButton) {
    setUsingNativeTitleBar(true);
    setContentOwned(new SetupComponent(), true);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
  }
  void closeButtonPressed() override { juce::JUCEApplication::quit(); }
};

juce::String option(const juce::StringArray& args, const char* name) {
  const int i = args.indexOf(name);
  return i >= 0 && i + 1 < args.size() ? args[i + 1].unquoted() : juce::String();
}

class SetupApplication final : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override { return "Core Setup"; }
  const juce::String getApplicationVersion() override { return kVersion; }
  bool moreThanOneInstanceAllowed() override { return false; }
  void initialise(const juce::String& commandLine) override {
    const juce::StringArray args = juce::StringArray::fromTokens(commandLine, true);
    const bool shortcuts = !args.contains("--no-shortcuts");
    if (args.contains("--install") || args.contains("--uninstall")) {
      const juce::String given = option(args, args.contains("--install") ? "--install" : "--uninstall");
      const juce::File self = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
      const juce::File folder = given.isNotEmpty() && !given.startsWith("--") ? juce::File(given)
                              : args.contains("--uninstall") && isCoreFolder(self.getParentDirectory()) ? self.getParentDirectory()
                                                                                                         : defaultFolder();
      juce::String problem = args.contains("--install") ? install(folder, shortcuts, false)
                                                        : uninstall(folder, shortcuts, samplesSource(option(args, "--samples")));
      if (problem.isEmpty() && args.contains("--install") && !args.contains("--no-samples"))
        problem = copySamples(samplesSource(option(args, "--samples")), folder, nullptr);
      if (problem.isNotEmpty()) std::fputs((problem + "\n").toRawUTF8(), stderr);
      setApplicationReturnValue(problem.isEmpty() ? 0 : 1);
      quit();
      return;
    }
    window_ = std::make_unique<SetupWindow>();
  }
  void shutdown() override { window_.reset(); }
private:
  std::unique_ptr<SetupWindow> window_;
};

}  // namespace

START_JUCE_APPLICATION(SetupApplication)
