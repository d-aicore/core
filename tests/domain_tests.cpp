// Testes do dominio (core_domain): o documento da app, guardar/reabrir, undo/redo, templates e a projeccao para o motor.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <utility>
#ifdef _WIN32
#include <share.h>
#endif
#include "support/check.hpp"
#include "core/project.hpp"
#include "core/server.hpp"
#include "edit.hpp"
#include "history.hpp"
#include "pattern_rack.hpp"
#include "plugin_refs.hpp"
#include "randomize.hpp"
#include "score.hpp"
#include "storage.hpp"
#include "templates.hpp"

using namespace core::doc;
using coretest::cases;
namespace fs = std::filesystem;

namespace {
fs::path tempDir() {
  fs::path d = fs::temp_directory_path() / "core-domain-tests";
  fs::create_directories(d);
  return d;
}
std::string canon(const Document& d) { return toJson(d).dump(); }
}  // namespace

TEST("documento: novo documento vazio e valido e estavel") {
  Document d = newEmptyDocument();
  CHECK_EQ(d.tracks.size(), (size_t)0);
  CHECK_EQ(canon(parseDocument(toJson(d))), canon(d));
}

TEST("documento: o template basico tem kick, bass e hats a espera de plugin") {
  Document d = newTemplateDocument();
  CHECK_EQ(d.tracks.size(), (size_t)3);
  for (const auto& t : d.tracks) CHECK(!t.patterns.empty() && !t.patterns[0].notes.empty());
}

TEST("guardar/reabrir: o ficheiro reaberto e igual ao guardado") {
  Document d = newTemplateDocument();
  d.name = "Teste guardar";
  d.bpm = 188;
  const fs::path file = tempDir() / ("reabrir" + std::string(kFileExtension));
  fs::remove(file);
  saveFile(d, file);
  CHECK(fs::exists(file));
  OpenResult r = openFile(file);
  CHECK(!r.imported);
  CHECK_EQ(canon(r.document), canon(d));
  // Guardar por cima deixa o anterior em .bak.
  d.bpm = 200;
  saveFile(d, file);
  CHECK(fs::exists(backupPath(file)));
  CHECK_NEAR(openFile(file).document.bpm, 200, 0);
}

TEST("guardar/reabrir: ficheiro estragado da erro e nao apaga nada") {
  const fs::path file = tempDir() / ("estragado" + std::string(kFileExtension));
  { std::FILE* f = std::fopen(file.string().c_str(), "wb"); std::fputs("{ isto nao e json", f); std::fclose(f); }
  CHECK_THROWS(openFile(file));
  CHECK(fs::exists(file));
}

// Perda de dados (2026-11-18), D3: o Core deixa abrir varias janelas e o Save gravava por cima do ficheiro sem ver
// se outra janela (ou outro programa) o tinha escrito entretanto; com uma so geracao de .bak, a segunda gravacao
// apagava de vez o trabalho da outra. A janela guarda o carimbo do ficheiro quando o abre e depois de cada gravacao
// (fileStamp) e o Save pergunta a diskChange() antes de escrever.
TEST("dados D3: o Save ve que o ficheiro mudou em disco desde que esta janela o abriu ou gravou") {
  const fs::path file = tempDir() / ("d3-outra-janela" + std::string(kFileExtension));
  fs::remove(file);
  fs::remove(backupPath(file));
  Document d = newTemplateDocument();
  d.name = "Janela 1";
  saveFile(d, file);
  const FileStamp mine = fileStamp(file);  // o que esta janela guarda depois de gravar (ou de abrir)
  CHECK(mine.exists);
  CHECK_EQ(mine.size, fs::file_size(file));
  CHECK_EQ(mine.sha256.size(), (size_t)64);

  // Ninguem lhe mexeu: igual, o Save grava sem perguntar.
  CHECK(diskChange(mine, file) == DiskChange::Same);

  // Outra janela gravou outra versao (outro conteudo, outro tamanho): mudou, o Save tem de perguntar.
  Document other = d;
  other.name = "Janela 2, com mais trabalho";
  other.bpm = 190;
  saveFile(other, file);
  CHECK(fs::file_size(file) != mine.size);
  CHECK(diskChange(mine, file) == DiskChange::Changed);

  // O mesmo tamanho, outro conteudo, e a data de modificacao reposta: e o conteudo que decide.
  saveFile(d, file);
  CHECK(diskChange(mine, file) == DiskChange::Same);  // (o mesmo conteudo outra vez: nada a perder, nao pergunta)
  const auto time = fs::last_write_time(file);
  std::string text = readTextFile(file);
  const size_t at = text.find("Janela 1");
  CHECK(at != std::string::npos);
  if (at != std::string::npos) text[at + 7] = '9';
  { std::FILE* f = std::fopen(file.string().c_str(), "wb"); std::fwrite(text.data(), 1, text.size(), f); std::fclose(f); }
  fs::last_write_time(file, time);
  CHECK_EQ(fs::file_size(file), mine.size);
  CHECK(diskChange(mine, file) == DiskChange::Changed);

  // So a data mudou (uma copia, um antivirus, uma sincronizacao que lhe tocou): igual.
  saveFile(d, file);
  fs::last_write_time(file, time + std::chrono::hours(1));
  CHECK(diskChange(mine, file) == DiskChange::Same);

  // Apagado por fora: nao ha nada para gravar por cima. O mais seguro e o Save voltar a cria-lo sem perguntar (a
  // cancao desta janela passa a ser a unica copia); por isso "Gone" e um estado a parte, nao "Changed".
  fs::remove(file);
  CHECK(diskChange(mine, file) == DiskChange::Gone);
  saveFile(d, file);
  CHECK(canon(openFile(file).document) == canon(d));
  CHECK(diskChange(fileStamp(file), file) == DiskChange::Same);

  // Um ficheiro que aparece onde esta janela nao conhecia nenhum (p.ex. uma cancao recuperada de um autosave cujo
  // ficheiro nao existia): e de outro, pergunta-se.
  const fs::path fresh = tempDir() / ("d3-nao-existia" + std::string(kFileExtension));
  fs::remove(fresh);
  const FileStamp none = fileStamp(fresh);
  CHECK(!none.exists);
  CHECK(diskChange(none, fresh) == DiskChange::Gone);
  saveFile(other, fresh);
  CHECK(diskChange(none, fresh) == DiskChange::Changed);
}

// Data loss (2026-11-18), D4: when ReplaceFileW fails with ERROR_UNABLE_TO_MOVE_REPLACEMENT_2 (1177) the original
// has already been renamed to the .bak and the new file did not get in. Core treated every error the same way:
// it deleted the temporary file (the only copy of the new version) and said "The previous file was not changed.".
namespace {
// What Windows leaves behind on 1177: the file being replaced already has the backup's name; the new one is not in.
unsigned long fail1177(const fs::path& target, const fs::path&, const fs::path& backup) {
  std::error_code e;
  fs::remove(backup, e);
  fs::rename(target, backup, e);
  return 1177;
}
// The same, and something else takes the name at once (a folder): the new file cannot be put in place either.
unsigned long fail1177NameTaken(const fs::path& target, const fs::path& temporary, const fs::path& backup) {
  fail1177(target, temporary, backup);
  fs::create_directory(target);
  return 1177;
}
struct SystemReplaceAgain { ~SystemReplaceAgain() { setReplaceStepForTests(nullptr); } };

// The temporary files a save left next to the file ("<file>.saving-N").
std::vector<fs::path> temporaries(const fs::path& file) {
  std::vector<fs::path> out;
  const std::string prefix = file.filename().string() + ".saving-";
  for (const auto& e : fs::directory_iterator(file.parent_path()))
    if (e.path().filename().string().rfind(prefix, 0) == 0) out.push_back(e.path());
  return out;
}
// The name of the song a file holds; "" when it is not there or does not open.
std::string songIn(const fs::path& file) {
  std::error_code e;
  if (!fs::is_regular_file(file, e)) return "";
  try { return openFile(file).document.name; } catch (const std::exception&) { return ""; }
}
fs::path cleanSong(const std::string& name) {
  const fs::path file = tempDir() / (name + std::string(kFileExtension));
  std::error_code e;
  fs::remove_all(file, e);
  fs::remove_all(backupPath(file), e);
  for (const auto& t : temporaries(file)) fs::remove_all(t, e);
  return file;
}
Document songNamed(const std::string& name) {
  Document d = newTemplateDocument();
  d.name = name;
  return d;
}
}  // namespace

TEST("data loss D4: ReplaceFileW fails half way (1177) and neither version is lost; the file opens") {
  const fs::path file = cleanSong("d4-1177");
  saveFile(songNamed("Version 1"), file);
  bool threw = false;
  {
    SystemReplaceAgain restore;
    setReplaceStepForTests(fail1177);
    try { saveFile(songNamed("Version 2"), file); } catch (const SaveError&) { threw = true; }
  }
  // The file is in its place and opens (before: it was not there, only the .bak was left).
  CHECK(fs::is_regular_file(file));
  CHECK(!songIn(file).empty());
  // Both versions are on disk: the new one in place (Core finished the swap itself), the previous one in the .bak.
  std::vector<std::string> onDisk = {songIn(file), songIn(backupPath(file))};
  for (const auto& t : temporaries(file)) onDisk.push_back(songIn(t));
  CHECK(std::find(onDisk.begin(), onDisk.end(), "Version 1") != onDisk.end());
  CHECK(std::find(onDisk.begin(), onDisk.end(), "Version 2") != onDisk.end());  // before: the temporary file had been deleted
  CHECK_EQ(songIn(file), std::string("Version 2"));
  CHECK_EQ(songIn(backupPath(file)), std::string("Version 1"));
  CHECK(!threw);  // the save is done: there is no error to show
  CHECK_EQ(temporaries(file).size(), (size_t)0);
}

TEST("data loss D4: 1177 and the new file cannot be moved in either -> the error says where each version is, the temporary file stays") {
  const fs::path file = cleanSong("d4-1177-name-taken");
  saveFile(songNamed("Version 1"), file);
  bool threw = false, intact = true;
  std::string message;
  {
    SystemReplaceAgain restore;
    setReplaceStepForTests(fail1177NameTaken);
    try { saveFile(songNamed("Version 2"), file); }
    catch (const SaveError& e) { threw = true; intact = e.previousFileIntact; message = e.what(); }
  }
  CHECK(threw);
  CHECK(!intact);  // the window can no longer say "The previous file was not changed."
  const std::vector<fs::path> left = temporaries(file);
  CHECK_EQ(left.size(), (size_t)1);  // before: 0, the new version had been deleted
  if (left.size() == 1) {
    CHECK_EQ(songIn(left[0]), std::string("Version 2"));
    CHECK(message.find(left[0].filename().string()) != std::string::npos);  // the error says where the new one is...
  }
  CHECK_EQ(songIn(backupPath(file)), std::string("Version 1"));
  CHECK(message.find(backupPath(file).filename().string()) != std::string::npos);  // ...and where the previous one is
  CHECK(message.find("(1177)") != std::string::npos);
  std::error_code e;
  fs::remove_all(file, e);  // (the folder that took the name)
}

TEST("data loss D4: the normal path is as it was (new in place, previous in the .bak, no temporary files)") {
  const fs::path file = cleanSong("d4-normal");
  saveFile(songNamed("Version 1"), file);  // the first save: there is no previous version
  CHECK_EQ(songIn(file), std::string("Version 1"));
  CHECK(!fs::exists(backupPath(file)));
  saveFile(songNamed("Version 2"), file);
  saveFile(songNamed("Version 3"), file);
  CHECK_EQ(songIn(file), std::string("Version 3"));
  CHECK_EQ(songIn(backupPath(file)), std::string("Version 2"));
  CHECK_EQ(temporaries(file).size(), (size_t)0);
}

#ifdef _WIN32
// Windows only (elsewhere an open file locks nothing): the .bak held open by another program.
TEST("data loss D4: locked .bak -> refused with an error, the previous file intact, no temporary file left, the error has words") {
  const fs::path file = cleanSong("d4-bak-locked");
  saveFile(songNamed("Version 1"), file);
  saveFile(songNamed("Version 2"), file);  // there is a .bak (Version 1)
  std::FILE* lock = _wfsopen(backupPath(file).c_str(), L"rb", _SH_DENYRW);
  CHECK(lock != nullptr);
  bool threw = false, intact = false;
  std::string message;
  try { saveFile(songNamed("Version 3"), file); }
  catch (const SaveError& e) { threw = true; intact = e.previousFileIntact; message = e.what(); }
  if (lock) std::fclose(lock);
  CHECK(threw);
  CHECK(intact);
  CHECK_EQ(songIn(file), std::string("Version 2"));
  CHECK_EQ(songIn(backupPath(file)), std::string("Version 1"));
  CHECK_EQ(temporaries(file).size(), (size_t)0);
  // The system's words for the error: a Windows without the English messages gave "error 1175 (1175)".
  std::printf("     (message: %s)\n", message.c_str());
  CHECK(message.find(": error ") == std::string::npos);
  CHECK(message.find("Cannot replace") == 0);
}
#endif

// Data loss (2026-11-18), BUG-SWEEP-01: the ".saving-N" counter starts at zero in every process. A save that failed
// half way through the replacement keeps its temporary file as the new version's only copy (D4 above); after a
// restart the next save of the same file picks the same name, truncates that retained recovery copy and — when the
// save is rejected — deletes it. Even a successful save consumed it (ReplaceFileW moves the temporary file into the
// target's place). A save may only write, move or delete a temporary file it created itself.
namespace {
// Seeds a range of retained recovery copies with distinct bytes. The suite's counter is already advanced (and is
// zero again in a fresh process), so the colliding name can be anywhere in the range.
std::vector<std::pair<fs::path, std::string>> seedRecoveryCopies(const fs::path& file) {
  std::vector<std::pair<fs::path, std::string>> seeds;
  for (int n = 0; n < 64; ++n) {
    const fs::path kept = file.parent_path() / (file.filename().string() + ".saving-" + std::to_string(n));
    const std::string bytes = "retained recovery copy " + std::to_string(n) + ": the only copy of a newer version\n";
    { std::ofstream out(kept, std::ios::binary | std::ios::trunc); out << bytes; }
    seeds.emplace_back(kept, bytes);
  }
  return seeds;
}
}  // namespace

TEST("data loss BUG-SWEEP-01: a rejected save leaves every retained recovery copy byte-identical") {
  const fs::path file = cleanSong("sweep01-rejected");
  { std::ofstream out(file, std::ios::binary); out << "the song on disk"; }
  const auto seeds = seedRecoveryCopies(file);
  bool threw = false;
  try { writeFileSafely("a new version", file, [](const std::string&) { return false; }); }
  catch (const SaveError&) { threw = true; }
  CHECK(threw);
  CHECK_EQ(readTextFile(file), std::string("the song on disk"));  // the previous file is intact
  for (const auto& [kept, bytes] : seeds) {
    CHECK(fs::is_regular_file(kept));  // before: exactly one was truncated and then deleted
    if (fs::is_regular_file(kept)) CHECK_EQ(readTextFile(kept), bytes);
  }
  CHECK_EQ(temporaries(file).size(), seeds.size());  // the save's own temporary file was discarded, nothing else
}

TEST("data loss BUG-SWEEP-01: a successful save does not consume a retained recovery copy") {
  const fs::path file = cleanSong("sweep01-success");
  { std::ofstream out(file, std::ios::binary); out << "the song on disk"; }
  const auto seeds = seedRecoveryCopies(file);
  writeFileSafely("a new version", file, [](const std::string&) { return true; });
  CHECK_EQ(readTextFile(file), std::string("a new version"));
  CHECK_EQ(readTextFile(backupPath(file)), std::string("the song on disk"));
  for (const auto& [kept, bytes] : seeds) {
    CHECK(fs::is_regular_file(kept));  // before: ReplaceFileW moved the retained copy into the target's place
    if (fs::is_regular_file(kept)) CHECK_EQ(readTextFile(kept), bytes);
  }
  CHECK_EQ(temporaries(file).size(), seeds.size());
}

TEST("undo/redo: adicionar e remover pistas volta exactamente atras") {
  Document start = newEmptyDocument();
  History h(start);
  CHECK(!h.canUndo());
  Document a = h.current();
  const std::string id = addTrack(a, "kick", "Kick");
  h.commit(a, "adicionar pista");
  Document b = h.current();
  addTrack(b, "bass", "Bass");
  h.commit(b, "adicionar pista");
  CHECK_EQ(h.current().tracks.size(), (size_t)2);
  CHECK(h.undo());
  CHECK_EQ(canon(h.current()), canon(a));
  CHECK(h.undo());
  CHECK_EQ(canon(h.current()), canon(start));
  CHECK(!h.canUndo());
  CHECK(h.redo());
  CHECK(h.redo());
  CHECK_EQ(canon(h.current()), canon(b));
  CHECK(!h.canRedo());
  // Uma alteracao nova depois de undo apaga o redo.
  CHECK(h.undo());
  Document c = h.current();
  removeTrack(c, id);
  h.commit(c, "remover pista");
  CHECK(!h.canRedo());
  CHECK_EQ(h.current().tracks.size(), (size_t)0);
}

TEST("undo/redo: um gesto (arrastar) e um so passo") {
  History h(newEmptyDocument());
  h.beginGesture("bpm");
  for (double bpm : {150.0, 160.0, 170.0}) { Document d = h.current(); d.bpm = bpm; h.updateGesture(d); }
  h.endGesture();
  CHECK_NEAR(h.current().bpm, 170, 0);
  CHECK(h.undo());
  CHECK_NEAR(h.current().bpm, 145, 0);
  CHECK(!h.canUndo());
}

TEST("modificado: guardar limpa, editar suja, undo ate ao guardado limpa") {
  History h(newEmptyDocument());
  h.markSaved();
  CHECK(!h.modified());
  Document d = h.current(); d.bpm = 190; h.commit(d, "bpm");
  CHECK(h.modified());
  h.undo();
  CHECK(!h.modified());
}

// A plugin state of 12 MB is in every snapshot: the 64 MB budget alone left 5 undo steps after 12 edits.
TEST("undo/redo: a large plugin state does not shrink the history below the minimum number of steps") {
  Document start = newEmptyDocument();
  addPluginTrack(start, {{"id", "big-state"}, {"file", "big-state.vst3"}, {"state", std::string(12u << 20, 'A')}});
  History h(start);
  for (int i = 0; i < 12; ++i) {
    Document d = h.current();
    d.bpm = 150 + i;
    h.commit(d, "bpm " + std::to_string(i));
  }
  int steps = 0;
  while (h.undo()) ++steps;
  CHECK_EQ(steps, 12);
  CHECK_NEAR(h.current().bpm, start.bpm, 0);
}

// Past the minimum, the byte budget still decides: the history does not grow without limit.
TEST("undo/redo: beyond the minimum number of steps the byte budget still drops the oldest") {
  Document start = newEmptyDocument();
  addPluginTrack(start, {{"id", "big-state"}, {"file", "big-state.vst3"}, {"state", std::string(1u << 20, 'A')}});
  History h(start, 8u << 20);
  for (int i = 0; i < History::kMinSteps + 20; ++i) {
    Document d = h.current();
    d.bpm = 100 + i;
    h.commit(d, "bpm " + std::to_string(i));
  }
  int steps = 0;
  while (h.undo()) ++steps;
  CHECK_EQ(steps, History::kMinSteps);
}

// The minimum number of steps has a ceiling of its own: 32 plugins with 4 MB of state each are 128 MB a step, and
// 50 of them would be 6.4 GB. Running out of memory loses more than a shorter undo does.
TEST("undo/redo: above the hard ceiling the oldest steps go even below the minimum, but one undo always stays") {
  Document start = newEmptyDocument();
  addPluginTrack(start, {{"id", "big-state"}, {"file", "big-state.vst3"}, {"state", std::string(1u << 20, 'A')}});
  auto stepsAfter = [&](int edits, size_t maxBytes, size_t hardMaxBytes) {
    History h(start, maxBytes, hardMaxBytes);
    for (int i = 0; i < edits; ++i) {
      Document d = h.current();
      d.bpm = 100 + i;
      h.commit(d, "bpm " + std::to_string(i));
    }
    int steps = 0;
    while (h.undo()) ++steps;
    CHECK_NEAR(h.current().bpm, steps == edits ? start.bpm : 100 + edits - 1 - steps, 0);  // the newest steps are the ones kept
    return steps;
  };
  // Below the budget nothing goes; between the budget and the ceiling the minimum stays (each step is 1 MB and a bit).
  CHECK_EQ(stepsAfter(5, 8u << 20, 20u << 20), 5);
  CHECK_EQ(stepsAfter(15, 8u << 20, 20u << 20), 15);
  // Above the ceiling: what fits in it, fewer than the minimum.
  CHECK_EQ(stepsAfter(30, 8u << 20, 20u << 20), 19);
  // A single step heavier than the ceiling: the current state and one undo, never less.
  CHECK_EQ(stepsAfter(6, 256u << 10, 512u << 10), 1);
}

// Data loss (2026-11-18): what a plugin holds is only in the engine. A change that takes a plugin away must be
// known before it is made, so the app can ask the engine for the states first (Undo then brings the sound back).
TEST("plugins: droppedPluginKeys names the plugins a change takes away, and nothing for other changes") {
  const json synth = {{"id", "synth-a"}, {"file", "synth-a.vst3"}, {"name", "Synth A"}};
  const json other = {{"id", "synth-b"}, {"file", "synth-b.vst3"}, {"name", "Synth B"}};
  const json effect = {{"id", "effect-a"}, {"file", "effect-a.vst3"}, {"name", "Effect A"}};
  Document before = newEmptyDocument();
  const std::string a = addPluginTrack(before, synth), b = addPluginTrack(before, synth);
  addPluginEffect(before, a, effect);
  addPluginEffect(before, "", effect);
  const std::string fxKey = pluginKey(a, before.tracks[0].effects[0]["id"].get<std::string>());
  const std::string masterKey = pluginKey("", before.masterEffects[0]["id"].get<std::string>());
  using Keys = std::vector<std::string>;
  auto sorted = [](Keys keys) { std::sort(keys.begin(), keys.end()); return keys; };

  // Removing a track: its instrument and its effects.
  Document removed = before;
  removeTrack(removed, a);
  CHECK(sorted(droppedPluginKeys(before, removed)) == sorted(Keys{pluginKey(a), fxKey}));
  // Another instrument on the track: the old one is let go; the same plugin again is not a change.
  Document replaced = before;
  replaceInstrumentWithPlugin(replaced, b, other);
  CHECK(droppedPluginKeys(before, replaced) == Keys{pluginKey(b)});
  Document same = before;
  replaceInstrumentWithPlugin(same, b, synth);
  CHECK(droppedPluginKeys(before, same).empty());
  // A sample on a plugin track (setTrackSample): the track's instrument is no longer the plugin.
  Document sampled = before;
  sampled.tracks[1].instrument.kind = "sampler";
  sampled.tracks[1].instrument.plugin = json();
  CHECK(droppedPluginKeys(before, sampled) == Keys{pluginKey(b)});
  // An effect removed, on a track and on the master.
  Document noEffect = before;
  removeEffect(noEffect, a, 0);
  removeEffect(noEffect, "", 0);
  CHECK(sorted(droppedPluginKeys(before, noEffect)) == sorted(Keys{fxKey, masterKey}));

  // Nothing is taken away by notes, the mixer, a new plugin, a duplicated track or a new state.
  Document notes = before;
  notes.tracks[0].patterns[0].notes.push_back(Note{});
  notes.tracks[0].mix.gainDb = -6;
  notes.bpm = 150;
  CHECK(droppedPluginKeys(before, notes).empty());
  Document more = before;
  addPluginTrack(more, other);
  addPluginEffect(more, b, effect);
  duplicateTrack(more, a);
  CHECK(droppedPluginKeys(before, more).empty());
  Document state = before;
  CHECK_EQ(applyPluginStates(state, {{pluginKey(a), "QQ=="}, {fxKey, "Qg=="}}), 2);
  CHECK(droppedPluginKeys(before, state).empty());
  CHECK(droppedPluginKeys(before, before).empty());
  // The other way round (Undo of a change that added plugins takes them away again).
  CHECK(sorted(droppedPluginKeys(removed, before)).empty());
  CHECK(droppedPluginKeys(more, before).size() == (size_t)4);  // the new track, its copy's instrument and effect, b's effect
}

// Data loss (2026-11-18), S5: an undo step is a whole document, plugin states included. Undo put the old state of
// a plugin that stays alive back into the document while the engine kept the instance and its new sound: the next
// save without the engine's answer, or an engine restart, brought the old sound.
TEST("plugins: keepLivePluginStates never takes back the state of a plugin that stays alive") {
  const json synth = {{"id", "synth-a"}, {"file", "synth-a.vst3"}, {"name", "Synth A"}};
  const json other = {{"id", "synth-b"}, {"file", "synth-b.vst3"}, {"name", "Synth B"}, {"state", "T1RIRVI="}};
  const json effect = {{"id", "effect-a"}, {"file", "effect-a.vst3"}, {"name", "Effect A"}};
  // The state Undo goes to: three synth tracks, an effect on the first, each with the state it had then.
  Document to = newEmptyDocument();
  const std::string a = addPluginTrack(to, synth), b = addPluginTrack(to, synth), c = addPluginTrack(to, synth);
  addPluginEffect(to, a, effect);
  const std::string fxKey = pluginKey(a, to.tracks[0].effects[0]["id"].get<std::string>());
  CHECK_EQ(applyPluginStates(to, {{pluginKey(a), "QQ=="}, {pluginKey(b), "Qg=="}, {pluginKey(c), "Qw=="}, {fxKey, "RA=="}}), 4);
  // The state being left: a and its effect moved on, b plays another plugin, c is gone.
  Document from = to;
  CHECK_EQ(applyPluginStates(from, {{pluginKey(a), "TkVX"}, {fxKey, "TkVXMg=="}}), 2);
  from.tracks[0].instrument.plugin["stateId"] = "s2";
  replaceInstrumentWithPlugin(from, b, other);
  removeTrack(from, c);
  from.bpm = 150;
  auto stateOf = [](const Document& d, const std::string& trackId) { return d.findTrack(trackId)->instrument.plugin.value("state", std::string()); };

  Document undone = to;
  CHECK_EQ(keepLivePluginStates(from, undone), 2);
  CHECK_EQ(stateOf(undone, a), std::string("TkVX"));  // alive on both sides: the newest state
  CHECK_EQ(undone.tracks[0].instrument.plugin.value("stateId", std::string()), std::string("s2"));
  CHECK_EQ(undone.tracks[0].effects[0]["plugin"].value("state", std::string()), std::string("TkVXMg=="));
  CHECK_EQ(stateOf(undone, b), std::string("Qg=="));  // another plugin under the key: the step's own state
  CHECK_EQ(undone.findTrack(b)->instrument.plugin.value("id", std::string()), std::string("synth-a"));
  CHECK_EQ(stateOf(undone, c), std::string("Qw=="));  // brought back by the step: the step's own state
  CHECK_NEAR(undone.bpm, to.bpm, 0);                  // nothing else of the state being left comes along

  // The same the other way (Redo), and a second time changes nothing.
  Document redone = from;
  CHECK_EQ(keepLivePluginStates(undone, redone), 0);
  CHECK_EQ(canon(redone), canon(from));
  CHECK_EQ(keepLivePluginStates(from, undone), 0);
  // A plugin with no state in the state being left takes nothing away.
  Document bare = from;
  bare.tracks[0].instrument.plugin.erase("state");
  bare.tracks[0].instrument.plugin.erase("stateId");
  Document kept = to;
  CHECK_EQ(keepLivePluginStates(bare, kept), 1);  // (the effect only)
  CHECK_EQ(stateOf(kept, a), std::string("QQ=="));
}

// Todos os templates que vao com a app abrem, viram projecto do motor, e o motor aceita-os e renderiza sem NaN.
TEST("templates: todos abrem e o motor toca-os sem erro") {
  TemplateLibrary lib = loadTemplates(CORE_TEMPLATES_DIR, tempDir() / "user-templates-vazia", true);
  for (const auto& p : lib.problems) coretest::fail(p.file.string().c_str(), 0, p.message);
  CHECK(lib.templates.size() >= 1);
  for (const auto& e : lib.templates) {
    Document d = documentFromTemplate(e.tmpl);
    core::Project p;
    try {
      p = core::fromJson(toProjection(d));
    } catch (const std::exception& ex) {
      coretest::fail(e.file.string().c_str(), 0, std::string("o motor recusou: ") + ex.what());
      continue;
    }
    p.lengthTicks = std::min<int64_t>(p.lengthTicks, 4 * 4 * core::kPpq);  // 4 compassos chegam
    core::RenderResult r = core::renderProject(p, 48000, 512, false, 0.1, nullptr, nullptr, nullptr, 0);
    bool finite = true;
    for (float s : r.L) finite = finite && std::isfinite(s);
    CHECK(finite);
  }
}

TEST("plugins por fabricante: todos entram num grupo, sem fabricante vai para Other, ordem alfabetica") {
  const json plugins = json::array({
      {{"format", "vst3"}, {"id", "VST3-Snap Heap-aaaa-1111"}, {"name", "Snap Heap"}, {"vendor", "KiloHearts"}, {"instrument", false}},
      {{"format", "vst3"}, {"id", "VST3-Multipass-bbbb-2222"}, {"name", "Multipass"}, {"vendor", "kiloHearts"}, {"instrument", false}},
      {{"format", "vst3"}, {"id", "VST3-Fracture-cccc-3333"}, {"name", "Fracture"}, {"vendor", "GlitchMachines"}, {"instrument", false}},
      {{"format", "vst3"}, {"id", "VST3-Kick3-dddd-4444"}, {"name", "Kick3"}, {"vendor", ""}, {"instrument", true}},
      {{"format", "vst3"}, {"id", "VST3-SemFabricante-eeee-5555"}, {"name", "Sem Fabricante"}, {"instrument", true}},
  });
  const auto groups = pluginsByVendor(plugins);
  size_t total = 0;
  for (const auto& g : groups) total += g.plugins.size();
  CHECK_EQ(total, plugins.size());  // nenhum plugin instalado se perde, mesmo sem fabricante
  CHECK_EQ(groups.size(), (size_t)3);
  if (groups.size() == 3) {
    CHECK_EQ(groups[0].vendor, std::string("GlitchMachines"));  // alfabetico, maiusculas ignoradas
    CHECK_EQ(groups[1].vendor, std::string("KiloHearts"));      // KiloHearts e kiloHearts: o mesmo fabricante
    CHECK_EQ(groups[1].plugins.size(), (size_t)2);
    CHECK_EQ(groups[2].vendor, std::string("Other"));           // sem fabricante: "Other", nao desaparece
    CHECK_EQ(groups[2].plugins.size(), (size_t)2);
  }
  CHECK(pluginsByVendor(json::object()).empty());  // entrada que nao e lista: vazio, sem erro
}

// ---- BUG-SWEEP-07: tempo round-trip (deeper audit 2026-11-18) --------------------------------------
// Ported from the Linux probe roundtrip.cpp lines 13-15 (archived verbatim in
// docs/relatorios/evidencia/2026-11-18-deeper-audit/): real setTempo 145 -> 300 -> 145 in separate
// edits returned the BPM but not the song: a 3500-tick bass gate became 1692 and the pattern count
// went 1 -> 3. Within one drag the UI resets from tempoBase_, so that path already round-tripped.
// This block is self-contained (own include and helpers) so parallel test streams rebase cleanly.
#include "randomize.hpp"

namespace {
// The probe's song: a bass track whose one 3840-tick pattern holds a single 3500-tick gate at tick 0.
Document tempoRoundTripSong() {
  Document d = newEmptyDocument();  // bpm 145, kPpq 960
  addTrack(d, "bass");
  Note n;
  n.id = "n1";
  n.length = 3500;
  d.tracks[0].patterns[0].notes.push_back(n);
  return d;
}
const Note& tempoActiveNote(const Document& d) {
  const Track& t = d.tracks[0];
  return t.findPattern(t.clips[0].patternId)->notes[0];
}
}  // namespace

TEST("tempo round-trip BUG-SWEEP-07: separate edits 145 -> 300 -> 145 restore the song exactly") {
  const Document base = tempoRoundTripSong();
  History h(base);
  Document up = h.current();
  setTempo(up, 300, {});
  h.commit(up, "Tempo");
  // One way, the ratified ms-gate contract holds (randomize.hpp:36-43, Diogo 2026-11-04): the gate
  // stays inside 80% of the 3840-tick gap, and the original pattern stays in the catalogue.
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)2);
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  Document down = h.current();
  setTempo(down, base.bpm, {});
  h.commit(down, "Tempo");
  std::printf("     (round-trip: length=%lld patterns=%zu equal=%d)\n",
              (long long)tempoActiveNote(h.current()).length, h.current().tracks[0].patterns.size(),
              (int)sameContent(base, h.current()));
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)1);
  CHECK(sameContent(base, h.current()));
  CHECK_NEAR(h.current().bpm, 145, 0);
  CHECK(!h.modified());
  CHECK(!h.canUndo());  // the exact round-trip cancels the Tempo step (history.cpp cancelsLastStep)
  CHECK(!h.canRedo());
}

TEST("tempo round-trip BUG-SWEEP-07: a single drag with the baseline reset restores the song") {
  const Document base = tempoRoundTripSong();
  History h(base);
  h.beginGesture("Tempo");
  // The UI resets every drag update from tempoBase_ (MainComponent.cpp:1891-1902, :2591-2599).
  Document up = base;
  setTempo(up, 300, {});
  h.updateGesture(up);
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)2);  // one copy, not one per drag step
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  Document back = base;
  setTempo(back, base.bpm, {});
  h.updateGesture(back);
  h.endGesture();
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)1);
  CHECK(sameContent(base, h.current()));
  CHECK(!h.modified());
  CHECK(!h.canUndo());
  CHECK(!h.canRedo());
}

TEST("tempo round-trip BUG-SWEEP-07: a walk 145 -> 300 -> 400 -> 145 still restores the original gates") {
  const Document base = tempoRoundTripSong();
  History h(base);
  for (double bpm : {300.0, 400.0}) {
    Document d = h.current();
    setTempo(d, bpm, {});
    h.commit(d, "Tempo");
    CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)2);  // the previous step's orphan copy is dropped
  }
  Document d = h.current();
  setTempo(d, base.bpm, {});
  h.commit(d, "Tempo");
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)1);
  CHECK(sameContent(base, h.current()));
  CHECK(!h.modified());
}

TEST("tempo round-trip BUG-SWEEP-07: an edited copy is retimed, never taken back to the source") {
  const Document base = tempoRoundTripSong();
  History h(base);
  Document up = h.current();
  setTempo(up, 300, {});
  h.commit(up, "Tempo");
  Document edited = h.current();
  Track& t = edited.tracks[0];
  t.findPattern(t.clips[0].patternId)->notes[0].length = 2000;  // the user shortens the gate in the copy
  h.commit(edited, "Edit note");
  Document down = h.current();
  setTempo(down, base.bpm, {});
  h.commit(down, "Tempo");
  // No exact restoration: the edit is kept and scaled by 145/300 (2000 -> 967); the original stays intact.
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)967);
  CHECK(!sameContent(base, h.current()));
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)2);  // original + new copy; the edited copy is orphaned
  CHECK_EQ(h.current().tracks[0].patterns[0].notes[0].length, (int64_t)3500);
}

TEST("tempo round-trip BUG-SWEEP-07: an edited source is never restored over the copy") {
  const Document base = tempoRoundTripSong();
  History h(base);
  Document up = h.current();
  setTempo(up, 300, {});
  h.commit(up, "Tempo");
  Document edited = h.current();
  edited.tracks[0].patterns[0].notes[0].length = 3600;  // the user edits the original in the catalogue
  h.commit(edited, "Edit note");
  Document down = h.current();
  setTempo(down, base.bpm, {});
  h.commit(down, "Tempo");
  CHECK(!sameContent(base, h.current()));
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)1692);  // normal retiming of the untouched copy
  CHECK_EQ(h.current().tracks[0].patterns[0].notes[0].length, (int64_t)3600);  // the edit survives
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)2);
}

TEST("tempo round-trip BUG-SWEEP-07: short gates keep their ms one way and come back exactly") {
  Document base = newEmptyDocument();
  addTrack(base, "bass");
  Note a; a.id = "n1"; a.tick = 0; a.length = 3500;
  Note b; b.id = "n2"; b.tick = 1920; b.length = 400;
  base.tracks[0].patterns[0].notes = {a, b};
  History h(base);
  Document up = h.current();
  setTempo(up, 300, {});
  h.commit(up, "Tempo");
  const Track& tu = h.current().tracks[0];
  const Pattern* cu = tu.findPattern(tu.clips[0].patternId);
  CHECK_EQ(cu->notes.size(), (size_t)2);
  CHECK_EQ(cu->notes[0].length, (int64_t)3500);  // clamped: the 80%-gap floor keeps the long gate
  CHECK_EQ(cu->notes[1].length, (int64_t)828);   // short gate scales with the BPM (its ms is kept)
  Document down = h.current();
  setTempo(down, base.bpm, {});
  h.commit(down, "Tempo");
  CHECK(sameContent(base, h.current()));
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  const Track& td = h.current().tracks[0];
  CHECK_EQ(td.findPattern(td.clips[0].patternId)->notes[1].length, (int64_t)400);
  CHECK_EQ(td.patterns.size(), (size_t)1);
}

TEST("tempo round-trip BUG-SWEEP-07: a saved tempo copy reopens and still restores exactly") {
  const Document base = tempoRoundTripSong();
  Document up = base;
  setTempo(up, 300, {});
  const fs::path file = tempDir() / ("tempo-roundtrip" + std::string(kFileExtension));
  fs::remove(file);
  saveFile(up, file);
  const Document reopened = openFile(file).document;
  CHECK(sameContent(up, reopened));  // the tempo metadata survives the reader (pattern extra)
  History h(reopened);
  Document down = h.current();
  setTempo(down, base.bpm, {});
  h.commit(down, "Tempo");
  CHECK(sameContent(base, h.current()));
  CHECK_EQ(tempoActiveNote(h.current()).length, (int64_t)3500);
  CHECK_EQ(h.current().tracks[0].patterns.size(), (size_t)1);
  fs::remove(file);
}

// BUG-SWEEP-29: tempo-only Randomize deleted Patterns-window rack-only ideas (no clip on the timeline).
// A slot's patterns are the tracks' own and need not be timeline clips (pattern_rack.hpp contract); an
// unarranged idea is the user's work and survives every Randomize axis without being sent to Arrange.
TEST("randomize: tempo-only keeps an unarranged Patterns-window idea (BUG-SWEEP-29)") {
  Document song = newTemplateDocument();
  song.extra["template"] = {{"style", "Dark psycore"}};
  addSection(song, "Groove", 0, song.lengthTicks);
  ensureRackSlots(song, 0);
  const std::string sid = addRackSlot(song, "Unarranged idea", 4);
  Note n;
  n.id = "n1";
  n.tick = 0;
  n.length = 960;
  n.pitch = 36;
  n.velocity = 0.5;
  n.hasRoll = true;
  n.roll.count = 4;
  n.roll.velocityFrom = 0.3;
  n.roll.velocityTo = 0.7;
  Pattern& idea = rackPatternFor(song, sid, "bass");
  const std::string pid = idea.id;
  idea.notes = {n};  // never sent to Arrange: no clip of it on the timeline
  CHECK(song.findTrack("bass")->findPattern(pid) != nullptr);
  CHECK_EQ(findRackSlot(rackSlots(song), sid)->tracks.size(), (size_t)1);
  const std::string before = canon(song);
  History history(song);
  RandomizeOptions o;
  o.axes = {"tempo"};
  const auto lines = randomizeStyle(song, o);
  CHECK(!lines.empty());  // tempo always yields at least one line, so the UI always commits this change
  // The idea pattern and the slot's track reference survive the transform.
  const Pattern* kept = song.findTrack("bass")->findPattern(pid);
  CHECK(kept != nullptr);
  if (kept) {
    CHECK_EQ(kept->notes.size(), (size_t)1);
    if (!kept->notes.empty()) {
      CHECK(kept->notes[0].hasRoll);
      CHECK_EQ(kept->notes[0].roll.count, 4);
    }
  }
  const auto slots = rackSlots(song);
  const PatternSlot* slot = findRackSlot(slots, sid);
  CHECK(slot != nullptr);
  if (slot) {
    CHECK_EQ(slot->tracks.size(), (size_t)1);
    CHECK_EQ(slot->tracks.count("bass"), (size_t)1);
  }
  // Reopening the slot shows the same pattern with its notes, not a fresh empty one.
  Pattern& reopened = rackPatternFor(song, sid, "bass");
  CHECK_EQ(reopened.id, pid);
  CHECK_EQ(reopened.notes.size(), (size_t)1);
  // Save/reopen round-trip keeps the idea and its slot reference.
  const Document reloaded = parseDocument(toJson(song));
  const Pattern* reloadedIdea = reloaded.findTrack("bass")->findPattern(pid);
  CHECK(reloadedIdea != nullptr);
  if (reloadedIdea) CHECK_EQ(reloadedIdea->notes.size(), (size_t)1);
  const auto reloadedSlots = rackSlots(reloaded);
  const PatternSlot* reloadedSlot = findRackSlot(reloadedSlots, sid);
  CHECK(reloadedSlot != nullptr);
  if (reloadedSlot) CHECK_EQ(reloadedSlot->tracks.count("bass"), (size_t)1);
  // Undo control: the committed Randomize goes back to the exact pre-transform state.
  history.commit(song, "Randomize tempo");
  CHECK(history.canUndo());
  CHECK(history.undo());
  CHECK(history.current().findTrack("bass")->findPattern(pid) != nullptr);
  CHECK_EQ(canon(history.current()), before);
}

// Characterization for BUG-LEGACY-RISE: the Stage Core panel's LIVE operations (Song shape, Arrange, the grid's
// cell on/off) keep working after the inactive scoreRises action left. These domain functions had no test before.
TEST("score: Stage Core's live operations (song shape, arrange, cell on/off) on the template song") {
  Document d = newTemplateDocument();
  const int64_t bar = 4 * kPpq;
  // Song shape: the sections laid out from bar 0, contiguous, in the shape's lengths; the song long enough.
  const ScoreShape* s = findScoreShape("darkpsycore-64");
  CHECK(s != nullptr);
  if (!s) return;
  setScoreShape(d, s->id);
  CHECK_EQ(d.sections.size(), s->stages.size());
  int64_t at = 0;
  for (size_t i = 0; i < d.sections.size(); ++i) {
    CHECK_EQ(d.sections[i].name, std::string(s->stages[i].name));
    CHECK_EQ(d.sections[i].startTick, at);
    CHECK_EQ(d.sections[i].lengthTicks, (int64_t)s->stages[i].bars * bar);
    at += d.sections[i].lengthTicks;
  }
  CHECK(d.lengthTicks >= at);
  CHECK_EQ(scoreStageOf("Build-up 2"), std::string("build"));
  CHECK_EQ(scoreStageOf("Drop 2"), std::string("drop"));
  CHECK_EQ(scoreStageOf("Rise"), std::string("build"));  // a section named "Rise" is still a build-up
  // The template's tracks keep their parts (the grid's row labels).
  CHECK_EQ(scorePartOf(trackById(d, "kick")), std::string("kick"));
  CHECK_EQ(scorePartOf(trackById(d, "bass")), std::string("bass"));
  CHECK_EQ(scorePartOf(trackById(d, "hats")), std::string("hats"));
  CHECK_EQ(scorePartName("openhats"), std::string("Open hats"));
  CHECK_EQ(scorePartName(""), std::string("Every section"));
  // Arrange: the rules write the clips (kick 5, bass 5, hats 5 over the six sections; the break rests).
  const size_t notesBefore = trackById(d, "kick").patterns.front().notes.size();
  const int written = scoreArrange(d, {"kick", "bass", "hats"});
  CHECK_EQ(written, 15);
  const Track& kick = trackById(d, "kick");
  const Track& bass = trackById(d, "bass");
  for (const auto& sec : d.sections) {
    const std::string stage = scoreStageOf(sec.name);
    if (stage == "break") CHECK(!scorePlays(kick, sec));
    if (stage == "drop" || stage == "roll") CHECK(scorePlays(kick, sec));
  }
  // The bar before the drop has no kick and no bass; the bass enters the intro at bar 4.
  const Section& build = d.sections[1], &intro = d.sections[0];
  const int64_t lastBar = build.startTick + build.lengthTicks - bar;
  bool kickInLastBar = false;
  for (const auto& c : kick.clips)
    if (c.startTick < build.startTick + build.lengthTicks && c.startTick + c.lengthTicks > lastBar) kickInLastBar = true;
  CHECK(!kickInLastBar);
  bool bassInFirstBars = false;
  for (const auto& c : bass.clips)
    if (c.startTick < intro.startTick + 4 * bar && c.startTick + c.lengthTicks > intro.startTick) bassInFirstBars = true;
  CHECK(!bassInFirstBars);
  CHECK(scorePlays(bass, intro));
  // A cell click: the track rests in the section, then plays again (one edit each, as the panel does).
  const std::string dropId = d.sections[2].id;
  CHECK(scorePlays(trackById(d, "kick"), d.sections[2]));
  scoreSetPlays(d, "kick", dropId, false);
  CHECK(!scorePlays(trackById(d, "kick"), d.sections[2]));
  scoreSetPlays(d, "kick", dropId, true);
  CHECK(scorePlays(trackById(d, "kick"), d.sections[2]));
  // Every clip still plays one of the track's own patterns with its notes (what the lane draws and the engine plays).
  CHECK_EQ(trackById(d, "kick").patterns.front().notes.size(), notesBefore);
  for (const auto& c : trackById(d, "kick").clips) {
    const Pattern* p = trackById(d, "kick").findPattern(c.patternId);
    CHECK(p != nullptr);
    if (p) CHECK(!p->notes.empty());
  }
  // The domain guards the panel never trips: an unknown shape, no track chosen, a track that plays no notes.
  bool threw = false;
  try { setScoreShape(d, "no-such-shape"); } catch (const DocumentError&) { threw = true; }
  CHECK(threw);
  threw = false;
  try { scoreArrange(d, {}); } catch (const DocumentError&) { threw = true; }
  CHECK(threw);
  const std::string audio = addAudioTrack(d, "audio");
  threw = false;
  try { scoreSetPlays(d, audio, dropId, true); } catch (const DocumentError&) { threw = true; }
  CHECK(threw);
}

// STATE-PER-EDIT (2026-11-18, work plan item 2): every edit projection carried the FULL state blob of every plugin
// (document.cpp toProjection copied t.instrument.plugin and the effect JSON verbatim), measured ~157 ms per edit with
// 32 tracks. The projection must name the state (stateId, and hasState for the places that have one) and leave the
// blob in the document: Save keeps writing it, and the engine asks for the keys it needs (project.load statesNeeded).
TEST("projection: plugin state does not travel with every edit (STATE-PER-EDIT)") {
  Document d = newEmptyDocument();
  const json synth = {{"id", "synth-a"}, {"file", "synth-a.vst3"}, {"name", "Synth A"}, {"format", "vst3"}};
  const std::string a = addPluginTrack(d, synth);
  const std::string b = addPluginTrack(d, synth);  // no state: a default plugin, nothing to ask for
  d.tracks[0].instrument.plugin["state"] = "QUJDRA==";
  d.tracks[0].instrument.plugin["stateId"] = "s1";
  addPluginEffect(d, a, {{"id", "fx-a"}, {"file", "fx-a.vst3"}, {"name", "FX A"}, {"format", "vst3"}});
  d.tracks[0].effects[0]["plugin"]["state"] = "RUZHSA==";
  d.masterEffects.push_back({{"id", "m1"}, {"type", "plugin"},
                             {"plugin", {{"id", "m-a"}, {"file", "m-a.vst3"}, {"format", "vst3"}, {"state", "TUFTVA=="}}}});
  const json p = toProjection(d);
  // No full blob anywhere in the projection (the 157 ms root).
  CHECK(p.dump().find("QUJDRA==") == std::string::npos);
  CHECK(p.dump().find("RUZHSA==") == std::string::npos);
  CHECK(p.dump().find("TUFTVA==") == std::string::npos);
  const json& pa = p["tracks"][0]["plugin"];
  CHECK(!pa.contains("state"));
  CHECK_EQ(pa.value("stateId", std::string()), std::string("s1"));  // the identity the engine keeps the instance by
  CHECK_EQ(pa.value("hasState", false), true);
  CHECK(!p["tracks"][1]["plugin"].contains("state"));
  CHECK(!p["tracks"][1]["plugin"].contains("hasState"));
  CHECK(!p["tracks"][0]["effects"][0]["plugin"].contains("state"));
  CHECK_EQ(p["tracks"][0]["effects"][0]["plugin"].value("hasState", false), true);
  CHECK(!p["masterEffects"][0]["plugin"].contains("state"));
  CHECK_EQ(p["masterEffects"][0]["plugin"].value("hasState", false), true);
  // The document keeps every blob: Save writes them, and they answer the engine's statesNeeded.
  const json saved = toJson(d);
  CHECK_EQ(saved["tracks"][0]["instrument"]["plugin"].value("state", std::string()), std::string("QUJDRA=="));
  CHECK_EQ(saved["tracks"][0]["effects"][0]["plugin"].value("state", std::string()), std::string("RUZHSA=="));
  CHECK_EQ(saved["master"]["effects"][0]["plugin"].value("state", std::string()), std::string("TUFTVA=="));
  // The engine accepts the projection's shape (hasState passes its validation).
  bool accepted = true;
  try { core::fromJson(p); } catch (const std::exception&) { accepted = false; }
  CHECK(accepted);
  // The keys of the song (what the app asks plugins.states for) and the states that answer statesNeeded.
  const std::vector<std::string> keys = pluginKeys(d);
  CHECK_EQ(keys.size(), (size_t)4);  // a, b, a's effect, the master's
  const std::string fxKey = pluginKey(a, d.tracks[0].effects[0]["id"].get<std::string>());
  const json states = pluginStatesFor(d, {pluginKey(a), pluginKey(b), fxKey, "fx:master:m1"});
  CHECK_EQ(states.size(), (size_t)3);  // b has no state: nothing to supply
  CHECK_EQ(states.value(pluginKey(a), std::string()), std::string("QUJDRA=="));
  CHECK_EQ(states.value(fxKey, std::string()), std::string("RUZHSA=="));
  CHECK_EQ(states.value("fx:master:m1", std::string()), std::string("TUFTVA=="));
  CHECK(!states.contains(pluginKey(b)));
  // applyPluginStates names the keys it changed: what the app pushes back to the engine before Save/Export.
  Document e = d;
  std::vector<std::string> changed;
  CHECK_EQ(applyPluginStates(e, {{pluginKey(a), "TkVX"}, {pluginKey(b), "QQ=="}}, &changed), 2);
  CHECK_EQ(changed.size(), (size_t)2);
  CHECK_EQ(changed[0], pluginKey(a));
  CHECK_EQ(changed[1], pluginKey(b));
  std::vector<std::string> unchanged;
  CHECK_EQ(applyPluginStates(e, {{pluginKey(a), "TkVX"}}, &unchanged), 0);
  CHECK(unchanged.empty());
}
CORE_TEST_MAIN
