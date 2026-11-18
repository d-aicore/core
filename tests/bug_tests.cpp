// Testes dos bugs medios e pequenos corrigidos a 2026-11-16 (lista do sweep antigo revista contra o main 06e0882).
// Cada teste falhava antes da correcao.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "support/check.hpp"
#include "assets.hpp"
#include "edit.hpp"
#include "follow.hpp"
#include "history.hpp"
#include "pattern_rack.hpp"
#include "pattern_tools.hpp"
#include "pcore.hpp"
#include "roles.hpp"
#include "samples.hpp"
#include "stutter.hpp"
#include "storage.hpp"
#include "templates.hpp"
#include "translation.hpp"
#include "core/project.hpp"
#include "core/tempo_map.hpp"
#include "MenuIds.h"

using namespace core::doc;
namespace fs = std::filesystem;
namespace menu = core::app::menu;

namespace {
constexpr int64_t kBar = 4 * kPpq;

fs::path freshDir(const std::string& name) {
  const fs::path d = fs::temp_directory_path() / "core-bug-tests" / name;
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}

void writeText(const fs::path& file, const std::string& text) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << text;
}

// Um WAV minimo (mono, 16 bit, 48 kHz, 64 amostras todas com `value`): valores diferentes, conteudos diferentes.
void tinyWav(const fs::path& file, int16_t value) {
  const uint32_t frames = 64, bytes = frames * 2, rate = 48000;
  auto u32 = [](uint32_t v) { return std::string{(char)(v & 255), (char)((v >> 8) & 255), (char)((v >> 16) & 255), (char)((v >> 24) & 255)}; };
  auto u16 = [](uint16_t v) { return std::string{(char)(v & 255), (char)((v >> 8) & 255)}; };
  std::string wav = "RIFF" + u32(36 + bytes) + "WAVEfmt " + u32(16) + u16(1) + u16(1) + u32(rate) + u32(rate * 2) + u16(2) + u16(16) +
                    "data" + u32(bytes);
  for (uint32_t i = 0; i < frames; ++i) wav += u16((uint16_t)value);
  writeText(file, wav);
}

bool lists(const std::vector<fs::path>& files, const fs::path& file) {
  return std::any_of(files.begin(), files.end(), [&](const fs::path& f) { std::error_code e; return fs::equivalent(f, file, e); });
}

// Uma cancao com um clip de audio, gravada em dir/<name>.core; devolve o ficheiro do audio dela em dir/Audio.
fs::path songWithAudio(const fs::path& dir, const std::string& name, int16_t value, Document* out = nullptr, std::string take = "") {
  const fs::path source = dir / ((take.empty() ? name + "-take" : take) + ".wav");
  tinyWav(source, value);
  Document d = newEmptyDocument();
  d.name = name;
  const std::string asset = importAudio(d, source, dir);
  addAudioClip(d, addAudioTrack(d, "take"), asset, 0);
  saveFile(d, dir / (name + kFileExtension));
  fs::remove(source);
  if (out) *out = d;
  return assetFile(*d.findAsset(asset), dir);
}

// BUG-SWEEP-16: the disk fails at the final flush, after the file was opened and written. The bytes reach
// the system and are then lost: what is on disk is an empty file, while the stream itself reports no error.
void lateFlushFault(std::ostream& out, const fs::path& file) {
  out.flush();
  std::ofstream lost(file, std::ios::binary | std::ios::trunc);
}
struct WriteFaultAgain { ~WriteFaultAgain() { setWriteFaultForTests(nullptr); } };
std::string bytesIn(const fs::path& file) {
  std::ifstream in(file, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
// Every regular file under the folder is non-empty: no truncated file left behind.
bool noEmptyFileUnder(const fs::path& dir) {
  for (auto it = fs::recursive_directory_iterator(dir); it != fs::recursive_directory_iterator(); ++it)
    if (it->is_regular_file() && fs::file_size(it->path()) == 0) return false;
  return true;
}
}  // namespace

// ---------------------------------------------------------------- perda de dados (2026-11-18), D1
// "Remove unused" mandava para a Reciclagem o audio das OUTRAS cancoes da pasta: todas partilham <pasta>/Audio e
// strayAudioFiles so olhava para a cancao aberta.

TEST("dados D1: o audio de outra cancao da mesma pasta nao e 'stray'") {
  const fs::path dir = freshDir("d1-duas-cancoes");
  Document a;
  const fs::path audioA = songWithAudio(dir, "A", 1000, &a);
  const fs::path audioB = songWithAudio(dir, "B", 2000);
  CHECK(fs::exists(audioA));
  CHECK(fs::exists(audioB));
  const auto stray = strayAudioFiles(a, dir);
  CHECK(!lists(stray, audioB));  // antes: la estava, e o Remove unused mandava-o para a Reciclagem
  CHECK(!lists(stray, audioA));
  CHECK_EQ(stray.size(), (size_t)0);
}

TEST("dados D1: um .core da pasta que nao se le -> nada e 'stray' (na duvida nao se apaga nada)") {
  for (const std::string broken : {"{ isto nao e json", R"({"format":"core-document","schemaVersion":99,"name":"de um Core mais novo"})",
                                   R"({"format":"outra-coisa"})"}) {
    const fs::path dir = freshDir("d1-ilegivel");
    Document a;
    songWithAudio(dir, "A", 1000, &a);
    songWithAudio(dir, "B", 2000);
    tinyWav(dir / kAssetFolder / "orfao.wav", 3000);  // ninguem o refere: sem o ficheiro estragado seria devolvido
    writeText(dir / "C.core", broken);
    CHECK_EQ(strayAudioFiles(a, dir).size(), (size_t)0);
    CHECK_EQ(filesNoSavedSongNeeds({dir / kAssetFolder / "orfao.wav"}, dir).size(), (size_t)0);
  }
  // O mesmo para a copia de seguranca de outra cancao: tambem e uma versao que alguem pode querer abrir.
  const fs::path dir = freshDir("d1-bak-ilegivel");
  Document a;
  songWithAudio(dir, "A", 1000, &a);
  tinyWav(dir / kAssetFolder / "orfao.wav", 3000);
  writeText(dir / "C.core.bak", "lixo");
  CHECK_EQ(strayAudioFiles(a, dir).size(), (size_t)0);
}

TEST("dados D1: um .wav que nenhuma cancao da pasta refere continua a ser devolvido") {
  const fs::path dir = freshDir("d1-orfao");
  Document a;
  songWithAudio(dir, "A", 1000, &a);
  songWithAudio(dir, "B", 2000);
  const fs::path orphan = dir / kAssetFolder / "orfao.wav";
  tinyWav(orphan, 3000);
  writeText(dir / "notas.json", R"({"isto":"nao e uma cancao do Core"})");  // um .json qualquer nao trava nada
  writeText(dir / "leia-me.txt", "nada");
  const auto stray = strayAudioFiles(a, dir);
  CHECK_EQ(stray.size(), (size_t)1);
  CHECK(lists(stray, orphan));
}

TEST("dados D1: contam tambem o .core.bak, um ficheiro de schema 1 e um 'core-project' da pasta") {
  const fs::path dir = freshDir("d1-outros-formatos");
  Document a;
  songWithAudio(dir, "A", 1000, &a);
  // B gravada duas vezes: a versao de agora ja nao tem o audio, a anterior (B.core.bak) ainda o tem.
  Document b;
  const fs::path audioB = songWithAudio(dir, "B", 2000, &b);
  Document later = b;
  later.tracks.clear();
  later.assets.clear();
  saveFile(later, dir / "B.core");
  CHECK(fs::exists(backupPath(dir / "B.core")));
  CHECK_EQ(openFile(dir / "B.core").document.assets.size(), (size_t)0);
  // Schema 1 (abre como importado): os assets dele contam.
  const fs::path audioOld = dir / kAssetFolder / "antigo.wav";
  tinyWav(audioOld, 4000);
  writeText(dir / "Antiga.core", R"({"format":"core-document","schemaVersion":1,"name":"Antiga","transport":{"bpm":145},"master":{},
    "assets":[{"id":"a1","name":"antigo.wav","path":"Audio/antigo.wav","sha256":"","sampleRate":48000,"channels":1,"frames":64}],"tracks":[]})");
  CHECK(openFile(dir / "Antiga.core").imported);
  // core-project (o formato do motor): o audio vai por caminho, relativo a pasta ou absoluto.
  const fs::path audioRel = dir / kAssetFolder / "do-motor.wav", audioAbs = dir / kAssetFolder / "do-motor-abs.wav";
  tinyWav(audioRel, 5000);
  tinyWav(audioAbs, 6000);
  const json clipRel = {{"path", "Audio/do-motor.wav"}, {"startTick", 0}, {"durationSeconds", 0.001}};
  const auto abs8 = audioAbs.u8string();
  const json clipAbs = {{"path", std::string(abs8.begin(), abs8.end())}, {"startTick", 0}, {"durationSeconds", 0.001}};
  const json project = {{"format", "core-project"}, {"version", 2}, {"bpm", 145}, {"lengthTicks", 960},
                        {"tracks", json::array({json{{"id", "audio1"}, {"instrument", "audio"}, {"audioClips", json::array({clipRel, clipAbs})}}})}};
  writeText(dir / "Motor.core", project.dump());
  const fs::path orphan = dir / kAssetFolder / "orfao.wav";
  tinyWav(orphan, 3000);
  const auto stray = strayAudioFiles(a, dir);
  CHECK(!lists(stray, audioB));
  CHECK(!lists(stray, audioOld));
  CHECK(!lists(stray, audioRel));
  CHECK(!lists(stray, audioAbs));
  CHECK(lists(stray, orphan));
  CHECK_EQ(stray.size(), (size_t)1);
}

TEST("dados D1: o mesmo audio importado em duas cancoes (um so ficheiro) nao sai quando uma delas o deixa de usar") {
  const fs::path dir = freshDir("d1-mesmo-conteudo");
  Document a;
  const fs::path audioA = songWithAudio(dir, "A", 1000, &a, "kick");
  const fs::path audioB = songWithAudio(dir, "B", 1000, nullptr, "kick");  // o mesmo ficheiro importado: um so em Audio
  CHECK(fs::equivalent(audioA, audioB));
  a.tracks.clear();  // em A ja nenhum clip o toca
  const std::vector<Asset> removed = removeUnusedAssets(a);
  CHECK_EQ(removed.size(), (size_t)1);
  std::vector<fs::path> candidates;
  for (const auto& r : removed) candidates.push_back(assetFile(r, dir));
  CHECK_EQ(filesNoSavedSongNeeds(candidates, dir).size(), (size_t)0);  // B (e a A que esta em disco) ainda o usam
  // Depois de A gravada sem ele, B continua a segura-lo.
  saveFile(a, dir / "A.core");
  saveFile(a, dir / "A.core");  // duas vezes: a copia de seguranca de A tambem ja nao o tem
  CHECK_EQ(strayAudioFiles(a, dir).size(), (size_t)0);
  CHECK_EQ(filesNoSavedSongNeeds(candidates, dir).size(), (size_t)0);
  // Sem B (nem a copia de seguranca dela) ja ninguem o usa.
  fs::remove(dir / "B.core");
  fs::remove(backupPath(dir / "B.core"));
  CHECK_EQ(filesNoSavedSongNeeds(candidates, dir).size(), (size_t)1);
  CHECK(lists(strayAudioFiles(a, dir), audioA));
}

TEST("dados D1: o texto da pergunta do Remove unused tem traducao") {
  const Translation pt = loadTranslation(CORE_LANG_DIR "/pt-PT.json");
  CHECK(pt.has("3 audio file(s) are not used: files of this song that no clip plays, and files in the Audio folder that no song "
               "of this folder uses.\n\nThe song's files leave the song (Edit > Undo puts them back in the song) and all of them go "
               "to the Recycle Bin, from where they can be restored. Files Undo can still bring back stay until the song is closed. "
               "Save the song afterwards."));
  // O aviso depois: a parte nova ("ou uma musica guardada desta pasta os usa") nao fica em ingles a meio da frase.
  for (const char* notice : {"2 unused audio file(s) sent to the Recycle Bin; 1 kept while Undo can bring them back or a saved song of this folder uses them.",
                             "2 unused audio file(s) sent to the Recycle Bin (1 could not be moved: in use?); 3 kept while Undo can bring them back or a "
                             "saved song of this folder uses them."}) {
    CHECK(pt.has(notice));
    CHECK(pt.translate(notice).find("kept while") == std::string::npos);
  }
}

// D3 (o teste da decisao esta em domain_tests.cpp): a pergunta do Save quando o ficheiro mudou em disco.
TEST("dados D3: a pergunta 'o ficheiro mudou em disco' tem traducao") {
  const Translation pt = loadTranslation(CORE_LANG_DIR "/pt-PT.json");
  for (const char* key : {"The file changed on disk", "Overwrite", "Cancel", "Save as\xE2\x80\xA6"}) CHECK(pt.has(key));
  const std::string asked = pt.translate(
      "\"Song.core\" was written by another window of Core or another program since this window opened or saved it.\n\n"
      "Overwrite replaces it with this window's song (the version on disk stays as the .bak backup). Save as\xE2\x80\xA6 keeps both.");
  CHECK(asked.rfind("\"Song.core\" foi escrito por outra janela do Core", 0) == 0);
  CHECK(asked.find("Substituir") != std::string::npos);
}

// D4 (the tests of the save itself are in domain_tests.cpp): the error that says where each version is, exactly as
// the domain writes it, has a translation.
namespace {
unsigned long failHalfWay(const fs::path& target, const fs::path&, const fs::path& backup) {
  std::error_code e;
  fs::remove(backup, e);
  fs::rename(target, backup, e);  // the previous file is already the .bak
  fs::create_directory(target);   // and the name is taken: the new one does not get in
  return 1177;
}
}  // namespace

TEST("data loss D4: the error 'the file is not in its place' has a translation") {
  const Translation pt = loadTranslation(CORE_LANG_DIR "/pt-PT.json");
  const fs::path dir = freshDir("d4-translation");
  const fs::path file = dir / "Song.core";
  Document d = newEmptyDocument();
  saveFile(d, file);
  d.bpm = 150;
  std::string message;
  setReplaceStepForTests(failHalfWay);
  try { saveFile(d, file); } catch (const SaveError& e) { message = e.what(); }
  setReplaceStepForTests(nullptr);
  CHECK(message.find("The file is not in its place: the new version is in ") != std::string::npos);
  CHECK(pt.has(message));
  const std::string shown = pt.translate(message);
  CHECK(shown.find("O ficheiro n\xC3\xA3o est\xC3\xA1 no s\xC3\xADtio: a vers\xC3\xA3o nova est\xC3\xA1 em ") != std::string::npos);
  CHECK(shown.find("Song.core.bak") != std::string::npos);
  CHECK(shown.find("Nothing was deleted") == std::string::npos);
  // The other two shapes (only the new version, only the previous one).
  CHECK(pt.has("Cannot replace C:\\x\\Song.core: Access is denied (5). The file is not in its place: the new version is in "
               "C:\\x\\Song.core.saving-3. Nothing was deleted: give it the name Song.core to open it."));
  CHECK(pt.has("Cannot replace C:\\x\\Song.core: Access is denied (5). The file is not in its place: the previous version is in "
               "C:\\x\\Song.core.bak. Nothing was deleted: give it the name Song.core to open it."));
}

// ---------------------------------------------------------------- data loss (2026-11-18), D5
// The user's sample catalogue (%APPDATA%\Core\samples-index.json) was made again from nothing when the index could
// not be read (another version of Core, a damaged part): the window started with no corrections and wrote the new
// index over the old file. The corrections made by hand (role, key, BPM, tags, names) were gone.
namespace {
const char* const kShaA = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char* const kShaB = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char* const kShaC = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";

// An index as a newer Core might write it: a version this Core refuses, with the user's corrections in it.
json futureIndex() {
  return {{"format", kSampleIndexFormat}, {"version", 99}, {"root", "D:/Samples"}, {"samples", json::array()},
          {"somethingNew", json{{"a", 1}}},
          {"overrides", json{{kShaA, json{{"role", "kick"}, {"bpm", 145}, {"key", "F#m"}, {"tags", json::array({"dark", "long"})}, {"name", "My kick"}}},
                             {kShaB, json{{"role", "bass"}, {"kind", "one-shot"}}}}}};
}
}  // namespace

TEST("data loss D5: the corrections of an index of another version are salvaged (the reader gives an error and none)") {
  const std::string text = futureIndex().dump(1);
  // What the reader does with this file: an error, so the window had no index and no corrections.
  CHECK_THROWS(indexFromJson(json::parse(text)));
  const std::map<std::string, json> saved = salvageOverrides(text);
  CHECK_EQ(saved.size(), (size_t)2);  // before: 0
  if (saved.size() == 2) {
    const json& a = saved.at(kShaA);
    CHECK_EQ(a.value("role", std::string()), std::string("kick"));
    CHECK_EQ(a.value("bpm", 0.0), 145.0);
    CHECK_EQ(a.value("key", std::string()), std::string("F#m"));
    CHECK_EQ(a.value("tags", json::array()).dump(), std::string(R"(["dark","long"])"));
    CHECK_EQ(a.value("name", std::string()), std::string("My kick"));
    CHECK_EQ(saved.at(kShaB).value("role", std::string()), std::string("bass"));
    CHECK_EQ(saved.at(kShaB).value("kind", std::string()), std::string("one-shot"));
  }
  // An index of this version with a damaged sample entry: the same (the reader refuses the whole file).
  json damaged = futureIndex();
  damaged["version"] = kSampleIndexVersion;
  damaged["samples"] = json::array({json{{"path", "kick.wav"}}});  // no "file", no "sha256"
  CHECK_THROWS(indexFromJson(damaged));
  CHECK_EQ(salvageOverrides(damaged.dump()).size(), (size_t)2);
}

TEST("data loss D5: salvaging is tolerant - what cannot be read is left out, and nothing throws") {
  // Cut short, not JSON, not an index, no corrections in it: none, without an exception.
  const std::string whole = futureIndex().dump(1);
  for (const std::string text : {whole.substr(0, whole.size() / 2), std::string("\x01\x02 not json at all"), std::string(), std::string("[1,2,3]"),
                                 std::string(R"({"format":"core-sample-index","version":99})"),
                                 std::string(R"({"format":"core-sample-index","version":99,"overrides":[1,2]})"),
                                 std::string(R"({"format":"core-sample-index","version":99,"overrides":"none"})")}) {
    bool threw = false;
    size_t found = 99;
    try { found = salvageOverrides(text).size(); } catch (...) { threw = true; }
    CHECK(!threw);
    CHECK_EQ(found, (size_t)0);
  }
  // Field by field: a field this Core does not know, or a value it cannot read, costs only itself.
  json mixed = futureIndex();
  mixed["overrides"] = json{{kShaA, json{{"role", "kick"}, {"mood", "angry"}, {"bpm", "fast"}}},  // role stays
                            {kShaB, json{{"role", "not-a-role"}}},                               // nothing readable
                            {kShaC, 42}};                                                        // not an object
  const std::map<std::string, json> saved = salvageOverrides(mixed.dump());
  CHECK_EQ(saved.size(), (size_t)1);
  if (saved.size() == 1) CHECK_EQ(saved.begin()->second.dump(), std::string(R"({"role":"kick"})"));
}

TEST("data loss D5: the index that could not be read is set aside, never written over, and the new one has the corrections") {
  const fs::path dir = freshDir("d5-set-aside");
  const fs::path file = dir / "samples-index.json";
  const std::string old = futureIndex().dump(1);
  writeText(file, old);
  // What the window does now when the index is there and cannot be read:
  const std::map<std::string, json> saved = salvageOverrides(readTextFile(file));
  const fs::path aside = setUnreadableIndexAside(file, "20261118-022300");
  CHECK_EQ(aside.filename().string(), std::string("samples-index.unreadable-20261118-022300.json"));
  CHECK(!fs::exists(file));                 // the name is free for the new index
  CHECK_EQ(readTextFile(aside), old);       // the old one is whole, byte for byte
  SampleIndex fresh;                        // (the scan makes it; here an empty library)
  fresh.root = "D:/Samples";
  fresh.overrides = saved;
  writeText(file, indexToJson(fresh).dump(1));
  CHECK_EQ(indexFromJson(json::parse(readTextFile(file))).overrides.size(), (size_t)2);  // before: 0, and the old file gone
  CHECK_EQ(readTextFile(aside), old);
  // It happens again in the same second: the first one set aside is not replaced.
  writeText(file, "broken again");
  const fs::path second = setUnreadableIndexAside(file, "20261118-022300");
  CHECK(!second.empty());
  CHECK(second != aside);
  CHECK_EQ(readTextFile(aside), old);
  CHECK_EQ(readTextFile(second), std::string("broken again"));
  // No file: nothing to set aside.
  CHECK(setUnreadableIndexAside(dir / "none.json", "20261118-022300").empty());
}

TEST("data loss D5: what the window says about an index it could not read has a translation") {
  const Translation pt = loadTranslation(CORE_LANG_DIR "/pt-PT.json");
  const std::string kept = pt.translate("Sample index not read (index: made by another version of Core): it is kept as "
                                        "samples-index.unreadable-20261118-022300.json and a new one is made. 12 correction(s) of yours were read from it.");
  CHECK(kept.find("fica guardado como samples-index.unreadable-20261118-022300.json") != std::string::npos);
  CHECK(kept.find("12 corre\xC3\xA7") != std::string::npos);
  const std::string stuck = pt.translate("Sample index not read (index: damaged: x) and it could not be renamed: it stays as it is and no new "
                                         "index is written over it. 0 correction(s) of yours were read from it.");
  CHECK(stuck.find("nenhum \xC3\xADndice novo \xC3\xA9 escrito por cima") != std::string::npos);
  const std::string notSaved = pt.translate("Sample index not saved: the index that could not be read is still in its place and is not written "
                                            "over (C:\\Users\\x\\AppData\\Roaming\\Core\\samples-index.json)");
  CHECK(notSaved.find("continua no s\xC3\xADtio") != std::string::npos);
}

TEST("sweep: addPattern recusa um padrao de comprimento 0 (dividia por zero na projeccao)") {
  Document d = newEmptyDocument();
  const std::string id = addTrack(d, "", "X");
  CHECK_THROWS(addPattern(trackById(d, id), 0));
  CHECK_THROWS(addPattern(trackById(d, id), 65 * kBar));
  CHECK(!addPattern(trackById(d, id), kBar).empty());
}

TEST("sweep: ids enormes num ficheiro nao dao a volta ao gerar o seguinte") {
  CHECK_EQ(idNumber("99999999999999999999999"), 1000000000L);
  CHECK_EQ(idNumber("-5"), 0L);
  CHECK_EQ(idNumber("abc"), 0L);
  Pattern p;
  p.id = "p1";
  p.lengthTicks = kBar;
  Note n;
  n.id = "n99999999999999999999";
  p.notes.push_back(n);
  CHECK_EQ(newNoteId(p), std::string("n1000000001"));
}

TEST("sweep: um padrao aprovado guarda o 'approval' e volta a ler; um id longo com sufixo cabe nos 80") {
  PatternPreset p;
  p.id = std::string(79, 'a');
  p.name = "Teste";
  p.status = "approved";
  p.approval = "Diogo: gosto";
  p.lengthTicks = kBar;
  Note n;
  n.id = "n1";
  n.tick = 0;
  n.pitch = 36;
  p.notes.push_back(n);
  const PatternPreset back = presetFromJson(presetToJson(p));
  CHECK_EQ(back.approval, p.approval);
  const fs::path dir = fs::temp_directory_path() / "core-bug-tests-presets";
  fs::remove_all(dir);
  const PatternPreset first = savePatternPreset(dir, p, {});
  const PatternPreset second = savePatternPreset(dir, p, {first});
  CHECK(second.id.size() <= 80);
  CHECK(second.id != first.id);
  std::vector<std::string> problems;
  CHECK_EQ(loadPatternLibrary({{"user", dir}}, &problems).size(), (size_t)2);
  CHECK(problems.empty());
}

TEST("sweep: schemaVersion 2^32 + 1 de um template nao e lido como 1") {
  json t = {{"format", "core-template"}, {"schemaVersion", 4294967297LL}, {"id", "x"}};
  CHECK_THROWS(parseTemplate(t));
}

TEST("sweep: P-Core com numeros enormes nao tem cast indefinido (recusado ou limitado)") {
  json j = {{"mode", "create"}, {"variationEvery", 1e300}, {"rollBeats", -1e300}, {"breakSpacing", 1e300}};
  bool threw = false;
  try { (void)pcoreLayersFromJson(j); } catch (const DocumentError&) { threw = true; }
  CHECK(threw);  // fora do intervalo: recusado com mensagem, nao um int qualquer
}

TEST("sweep: isolateRegion guarda os campos extra do clip cortado e o nome da copia nao parte um caracter") {
  Document d = newEmptyDocument();
  d.lengthTicks = 8 * kBar;
  const std::string id = addTrack(d, "", "X");
  Track& t = trackById(d, id);
  t.patterns[0].name = std::string(36, 'a') + "\xC3\xA7\xC3\xA7";  // 40 bytes, acaba em 'cc' cedilhado
  t.clips = {{"c1", "p1", 0, 8 * kBar, 0, json{{"meu", 1}}}};
  isolateRegion(d, id, 2 * kBar, 4 * kBar, {});
  for (const auto& c : trackById(d, id).clips) CHECK_EQ(c.extra.value("meu", 0), 1);
  CHECK_EQ(toJson(parseDocument(toJson(d))).dump(), toJson(d).dump());  // nomes validos: guarda e reabre
}

TEST("sweep: STUTTER numa zona apanha as notas das repeticoes de um clip em loop") {
  Document d = newEmptyDocument();
  d.lengthTicks = 8 * kBar;
  const std::string id = addTrack(d, "", "Lead");
  Track& t = trackById(d, id);
  t.patterns[0].lengthTicks = kBar;
  Note n;
  n.id = "n1";
  n.tick = 0;
  n.length = kPpq;
  n.pitch = 60;
  t.patterns[0].notes = {n};
  t.clips = {{"c1", "p1", 0, 4 * kBar, 0, json::object()}};  // o compasso repete 4 vezes
  // Zona no 3o compasso: so a repeticao conta (antes: nada, porque a 1a passagem esta fora da zona).
  const int written = stutterTrack(d, id, 2 * kBar, 3 * kBar, stutterPresets().front());
  CHECK(written > 0);
}

TEST("sweep: FOLLOW grava o corte da nota que atravessa o inicio da zona") {
  Document d = newEmptyDocument();
  d.lengthTicks = 4 * kBar;
  const std::string kick = addTrack(d, "", "Kick");
  Track& t = trackById(d, kick);
  t.patterns[0].lengthTicks = 4 * kBar;
  Note longNote;
  longNote.id = "n1";
  longNote.tick = 0;
  longNote.length = 2 * kBar;  // toca ate meio do 3o compasso
  longNote.pitch = 36;
  t.patterns[0].notes = {longNote};
  t.clips = {{"c1", "p1", 0, 4 * kBar, 0, json::object()}};
  FollowZone z;
  z.start = kBar;
  z.end = kBar + kPpq;
  z.onsets = {kBar, kBar + kPpq / 4, kBar + kPpq / 2};
  kickFollow(d, kick, {z}, FollowSettings{});
  // A nota longa acaba onde a zona comeca, na musica (nao so na partitura de trabalho).
  bool cut = false, found = false;
  for (const auto& c : trackById(d, kick).clips) {
    const Pattern* p = trackById(d, kick).findPattern(c.patternId);
    for (const auto& note : p->notes)
      if (note.tick >= c.offsetTicks && c.startTick + note.tick - c.offsetTicks == 0 && note.pitch == 36) {
        found = true;
        cut = note.length <= kBar;
      }
  }
  CHECK(found);
  CHECK(cut);
}


// Audit 2026-11-17, item 4: duplicateSection pushed the tempo changes after the section but did not copy
// the ones inside it, so the duplicated section lost the tempo (and the ramp) randomize axis J writes.
// BUG-SWEEP-22 grew this: the copy also pins the section's effective start tempo (one more point).
TEST("duplicating a section copies the tempo changes inside it") {
  Document d = newEmptyDocument();
  const std::string a = addSection(d, "Build", 0, 4 * kBar);
  addSection(d, "Drop", 4 * kBar, 4 * kBar);
  setTempoMap(d, {{1 * kBar, 200, json::object(), false}, {3 * kBar, 160, json::object(), true}, {6 * kBar, 120, json::object(), false}});
  const size_t before = d.tempoMap.size();
  const std::string dup = duplicateSection(d, a);
  CHECK_EQ(d.tempoMap.size(), before + 3);  // the two interior points are copied, plus the copy's start pin
  const auto at = [&](int64_t tick) -> const TempoPoint* {
    for (const auto& p : d.tempoMap) if (p.tick == tick) return &p;
    return nullptr;
  };
  const TempoPoint* copy = at(5 * kBar);
  CHECK(copy != nullptr);
  if (copy) {
    CHECK_NEAR(copy->bpm, 200, 1e-9);
    CHECK(!copy->ramp);
  }
  const TempoPoint* glide = at(7 * kBar);
  CHECK(glide != nullptr);
  if (glide) {
    CHECK_NEAR(glide->bpm, 160, 1e-9);
    CHECK(glide->ramp);  // the ramp too
  }
  const TempoPoint* later = at(10 * kBar);  // what already went after moves on by one section
  CHECK(later != nullptr);
  if (later) CHECK_NEAR(later->bpm, 120, 1e-9);
  // The tempo playing in the duplicated section is the original section's, not the last one before it.
  CHECK_NEAR(bpmAt(d, 4 * kBar), 145, 1e-9);   // BUG-SWEEP-22: the copy starts at the song's own start tempo
  CHECK_NEAR(bpmAt(d, 5 * kBar), 200, 1e-9);   // before: 160, the 3-bar point, which kept playing
  CHECK_NEAR(bpmAt(d, 7 * kBar), 160, 1e-9);
  CHECK(!dup.empty());
  const Section* moved = nullptr;
  for (const auto& s : d.sections) if (s.id == dup) moved = &s;
  CHECK(moved != nullptr);
  if (moved) CHECK_EQ(moved->startTick, 4 * kBar);
}

// ---------------------------------------------------------------- work preservation (2026-11-18), BUG-SWEEP-17
// A save that fails half way through the replace keeps the new version as "<song>.core.saving-N" (storage.cpp,
// previousFileIntact=false). audioSavedSongsUse read .core/.core.bak/.json/.json.bak only: audio that only the
// retained recovery copy references was listed as stray, and "Remove unused" sent it to the Recycle Bin.
TEST("work preservation BUG-SWEEP-17: a retained .saving-N copy keeps the audio only it references") {
  const fs::path dir = freshDir("sweep17-saving-copy");
  Document a;
  const fs::path audioA = songWithAudio(dir, "A", 1000, &a);
  Document b = newEmptyDocument();  // B on disk has no audio yet
  b.name = "B";
  saveFile(b, dir / "B.core");
  // The new version of B imports X; the replace fails half way (the D4 shape): B.core's name is taken by a
  // directory, the previous version (no X) is the .bak, and the new one stays only in B.core.saving-N.
  const fs::path source = dir / "B-take.wav";
  tinyWav(source, 2000);
  const std::string asset = importAudio(b, source, dir);
  addAudioClip(b, addAudioTrack(b, "take"), asset, 0);
  const fs::path audioX = assetFile(*b.findAsset(asset), dir);
  fs::remove(source);
  bool threw = false;
  setReplaceStepForTests(failHalfWay);
  try { saveFile(b, dir / "B.core"); } catch (const SaveError&) { threw = true; }
  setReplaceStepForTests(nullptr);
  CHECK(threw);
  std::vector<fs::path> retained;
  for (fs::directory_iterator it(dir), end; it != end; ++it) {
    const std::string n = it->path().filename().string();
    if (n.rfind("B.core.saving-", 0) == 0) retained.push_back(it->path());
  }
  CHECK_EQ(retained.size(), (size_t)1);  // the new version's only copy is on disk
  CHECK(fs::exists(audioX));
  // X is referenced by no asset of the open song, no .core and no .bak: only the retained copy. It must stay.
  CHECK_EQ(strayAudioFiles(a, dir).size(), (size_t)0);  // before: X was listed -> Recycle Bin
  CHECK_EQ(filesNoSavedSongNeeds({audioX}, dir).size(), (size_t)0);
  CHECK(!lists(strayAudioFiles(a, dir), audioA));
  // A real orphan still goes: the fix must not freeze the cleanup (D1 Recycle-Bin semantics stay).
  const fs::path orphan = dir / kAssetFolder / "orphan.wav";
  tinyWav(orphan, 3000);
  const auto stray = strayAudioFiles(a, dir);
  CHECK_EQ(stray.size(), (size_t)1);
  CHECK(lists(stray, orphan));
  fs::remove_all(dir / "B.core");  // the directory that took the name
}

TEST("work preservation BUG-SWEEP-17: an unreadable retained .saving-N copy is doubt; a non-song copy does not block") {
  const fs::path dir = freshDir("sweep17-saving-doubt");
  Document a;
  songWithAudio(dir, "A", 1000, &a);
  const fs::path orphan = dir / kAssetFolder / "orphan.wav";
  tinyWav(orphan, 3000);
  // A retained copy named as a song that cannot be read: nobody knows what it needs, nothing goes.
  writeText(dir / "C.core.saving-4", "{ this is not json");
  CHECK_EQ(strayAudioFiles(a, dir).size(), (size_t)0);
  CHECK_EQ(filesNoSavedSongNeeds({orphan}, dir).size(), (size_t)0);
  writeText(dir / "C.core.saving-4", R"({"format":"core-document","schemaVersion":99,"name":"from a newer Core"})");
  CHECK_EQ(filesNoSavedSongNeeds({orphan}, dir).size(), (size_t)0);
  // A retained copy of something that is not a song (notes, a template) is not a song either; ".saving-" with
  // no number is not a save temporary.
  fs::remove(dir / "C.core.saving-4");
  writeText(dir / "notes.txt.saving-2", "anything");
  writeText(dir / "C.core.saving-", "anything");
  const auto stray = strayAudioFiles(a, dir);
  CHECK_EQ(stray.size(), (size_t)1);
  CHECK(lists(stray, orphan));
  // A retained copy of an engine-format project counts too: the wav paths in it keep their audio.
  fs::remove(dir / "notes.txt.saving-2");
  fs::remove(dir / "C.core.saving-");
  const json clip = {{"path", "Audio/orphan.wav"}, {"startTick", 0}, {"durationSeconds", 0.001}};
  const json project = {{"format", "core-project"}, {"version", 2}, {"bpm", 145}, {"lengthTicks", 960},
                        {"tracks", json::array({json{{"id", "audio1"}, {"instrument", "audio"}, {"audioClips", json::array({clip})}}})}};
  writeText(dir / "Motor.core.saving-9", project.dump());
  CHECK_EQ(filesNoSavedSongNeeds({orphan}, dir).size(), (size_t)0);
  CHECK_EQ(strayAudioFiles(a, dir).size(), (size_t)0);
}

// BUG-SWEEP-22..25 (external audit 2026-11-18, "report (3).md"): timeline edits must keep the tempo
// continuous. Each case below was measured red on ed4642e before its fix. Engine seconds come from the
// projection the app sends (core::fromJson + core::TempoMap at 48 kHz), the same conversion the engine plays.
namespace tempo_sweep {
double engineSeconds(const Document& doc, int64_t tick) {
  const core::Project p = core::fromJson(toProjection(doc));
  const core::TempoMap map(p.bpm, p.tempoMap, 48000);
  return map.sampleAt((double)tick) / 48000.0;
}
bool validSavedForm(const Document& doc) {
  try { (void)parseDocument(toJson(doc)); return true; } catch (...) { return false; }
}
}  // namespace tempo_sweep

TEST("BUG-SWEEP-22: a duplicated section starts at its own tempo, boundary ramps keep their shape") {
  using namespace tempo_sweep;
  // The probe's song: the copy used to inherit the original's final BPM (160) instead of its start (120).
  Document d = newEmptyDocument();
  d.bpm = 120;
  const std::string a = addSection(d, "A", 0, 4 * kBar);
  addSection(d, "B", 4 * kBar, 4 * kBar);
  setTempoMap(d, {{1 * kBar, 200, json::object(), false}, {3 * kBar, 160, json::object(), true}});
  const Document original = d;
  const double firstInterval = engineSeconds(original, 1 * kBar) - engineSeconds(original, 0);
  duplicateSection(d, a);
  CHECK_NEAR(bpmAt(d, 4 * kBar), 120, 1e-9);                        // before the fix: 160
  CHECK_NEAR(bpmAt(d, 5 * kBar), 200, 1e-9);
  CHECK_NEAR(bpmAt(d, 7 * kBar), 160, 1e-9);
  CHECK_NEAR(bpmAt(d, 8 * kBar), bpmAt(original, 4 * kBar), 1e-9);  // the follower keeps its effective start
  CHECK_NEAR(engineSeconds(d, 5 * kBar) - engineSeconds(d, 4 * kBar), firstInterval, 1e-4);
  CHECK(validSavedForm(d));

  // A glide crossing both edges of the section: the original keeps its curve, the copy is self-contained,
  // and the follower starts at the value the music had at the section's end. One extra step is not enough.
  Document g = newEmptyDocument();
  g.bpm = 120;
  const std::string ga = addSection(g, "A", 2 * kBar, 2 * kBar);
  addSection(g, "B", 4 * kBar, 4 * kBar);
  setTempoMap(g, {{6 * kBar, 240, json::object(), true}});
  const Document gOrig = g;
  duplicateSection(g, ga);
  for (int64_t t : {(int64_t)0, (int64_t)kBar, (int64_t)(2 * kBar), (int64_t)(3 * kBar), (int64_t)(4 * kBar - 1)})
    CHECK_NEAR(bpmAt(g, t), bpmAt(gOrig, t), 1e-6);
  CHECK_NEAR(bpmAt(g, 4 * kBar), bpmAt(gOrig, 2 * kBar), 1e-9);     // the copy's first tick
  CHECK_NEAR(bpmAt(g, 5 * kBar), bpmAt(gOrig, 3 * kBar), 1e-6);
  CHECK_NEAR(bpmAt(g, 6 * kBar - 1), bpmAt(gOrig, 4 * kBar - 1), 1e-6);
  CHECK_NEAR(bpmAt(g, 6 * kBar), bpmAt(gOrig, 4 * kBar), 1e-6);     // the follower's first tick
  CHECK_NEAR(bpmAt(g, 7 * kBar), bpmAt(gOrig, 5 * kBar), 1e-6);
  CHECK_NEAR(bpmAt(g, 8 * kBar), bpmAt(gOrig, 6 * kBar), 1e-6);
  CHECK_NEAR(engineSeconds(g, 6 * kBar) - engineSeconds(g, 4 * kBar),
             engineSeconds(gOrig, 4 * kBar) - engineSeconds(gOrig, 2 * kBar), 1e-4);
  CHECK(validSavedForm(g));

  // Step control: no interpolation involved, the copy still starts at the section's own start tempo.
  Document s = newEmptyDocument();
  s.bpm = 120;
  const std::string sa = addSection(s, "A", 0, 4 * kBar);
  addSection(s, "B", 4 * kBar, 4 * kBar);
  setTempoMap(s, {{1 * kBar, 200, json::object(), false}, {3 * kBar, 160, json::object(), false}});
  duplicateSection(s, sa);
  CHECK_NEAR(bpmAt(s, 4 * kBar), 120, 1e-9);                        // before the fix: 160
  CHECK_NEAR(bpmAt(s, 5 * kBar), 200, 1e-9);
  CHECK_NEAR(bpmAt(s, 8 * kBar), 160, 1e-9);
  CHECK(validSavedForm(s));

  // No tempo map: nothing is pinned, the constant tempo carries (a song without a map plays as before).
  Document e = newEmptyDocument();
  const std::string ea = addSection(e, "A", 0, 4 * kBar);
  duplicateSection(e, ea);
  CHECK(e.tempoMap.empty());
  CHECK_NEAR(bpmAt(e, 4 * kBar), e.bpm, 1e-9);
}

TEST("BUG-SWEEP-23: swapping sections moves the tempo with the music") {
  using namespace tempo_sweep;
  // The probe's song: the map used to stay byte-identical while the music exchanged places.
  Document d = newEmptyDocument();
  d.bpm = 120;
  const std::string a = addSection(d, "A", 0, 4 * kBar);
  addSection(d, "B", 4 * kBar, 4 * kBar);
  setTempoMap(d, {{4 * kBar, 240, json::object(), false}});
  const json mapBefore = toJson(d)["transport"]["tempoMap"];
  const Document original = d;
  swapSection(d, a, true);
  CHECK_EQ(d.sections[0].name, std::string("B"));
  CHECK_NEAR(bpmAt(d, 0), 240, 1e-9);                               // before the fix: 120 (B's music, A's tempo)
  CHECK_NEAR(bpmAt(d, 4 * kBar), 120, 1e-9);                        // before the fix: 240
  CHECK_NEAR(bpmAt(d, 8 * kBar), bpmAt(original, 8 * kBar), 1e-9);  // outside the swap: unchanged
  CHECK(toJson(d)["transport"]["tempoMap"] != mapBefore);
  CHECK_NEAR(engineSeconds(d, 4 * kBar), engineSeconds(original, 8 * kBar) - engineSeconds(original, 4 * kBar), 1e-4);
  CHECK(validSavedForm(d));

  // Unequal lengths (2 bars vs 4) with ramps inside both sections: translation keeps the ramp slopes.
  Document g = newEmptyDocument();
  g.bpm = 120;
  const std::string ga = addSection(g, "A", 0, 2 * kBar);
  addSection(g, "B", 2 * kBar, 4 * kBar);
  setTempoMap(g, {{1 * kBar, 180, json::object(), true}, {3 * kBar, 240, json::object(), false}, {5 * kBar, 150, json::object(), true}});
  const Document gOrig = g;
  swapSection(g, ga, true);
  CHECK_NEAR(bpmAt(g, 0), 180, 1e-9);                               // B's effective start becomes the song's start
  for (int64_t t : {(int64_t)0, (int64_t)kBar, (int64_t)(2 * kBar), (int64_t)(3 * kBar), (int64_t)(4 * kBar - 1)})
    CHECK_NEAR(bpmAt(g, t), bpmAt(gOrig, t + 2 * kBar), 1e-6);      // B's curve, translated by -aLen
  CHECK_NEAR(bpmAt(g, 4 * kBar), 120, 1e-9);                        // A's effective start pinned where A lands
  for (int64_t t : {(int64_t)(4 * kBar), (int64_t)(5 * kBar), (int64_t)(6 * kBar - 1)})
    CHECK_NEAR(bpmAt(g, t), bpmAt(gOrig, t - 4 * kBar), 1e-6);      // A's curve, translated by +bLen
  CHECK_NEAR(bpmAt(g, 6 * kBar), bpmAt(gOrig, 6 * kBar), 1e-9);
  CHECK_NEAR(bpmAt(g, 8 * kBar), bpmAt(gOrig, 8 * kBar), 1e-9);
  CHECK_NEAR(engineSeconds(g, 4 * kBar), engineSeconds(gOrig, 6 * kBar) - engineSeconds(gOrig, 2 * kBar), 1e-4);
  CHECK(validSavedForm(g));

  // Swapping with the previous section is the same exchange seen from the other side.
  Document h = newEmptyDocument();
  h.bpm = 120;
  addSection(h, "A", 0, 2 * kBar);
  const std::string hb = addSection(h, "B", 2 * kBar, 4 * kBar);
  setTempoMap(h, {{1 * kBar, 180, json::object(), true}, {3 * kBar, 240, json::object(), false}, {5 * kBar, 150, json::object(), true}});
  swapSection(h, hb, false);
  CHECK_EQ(toJson(h)["transport"]["tempoMap"].dump(), toJson(g)["transport"]["tempoMap"].dump());
  CHECK_NEAR(h.bpm, g.bpm, 1e-9);

  // No tempo map: the swap stays sound-neutral.
  Document e = newEmptyDocument();
  const std::string ea = addSection(e, "A", 0, 4 * kBar);
  addSection(e, "B", 4 * kBar, 4 * kBar);
  const double base = e.bpm;
  swapSection(e, ea, true);
  CHECK(e.tempoMap.empty());
  CHECK_NEAR(e.bpm, base, 1e-9);
}

TEST("BUG-SWEEP-24: inserted empty bars hold the tempo, the ramp keeps its shape on both sides") {
  using namespace tempo_sweep;
  // The probe's song: the whole ramp used to re-interpolate (150 -> 140 at bar 1, 1.78515 s -> 1.84981 s).
  Document d = newEmptyDocument();
  d.bpm = 120;
  setTempoMap(d, {{4 * kBar, 240, json::object(), true}});
  const Document original = d;
  insertTime(d, 2 * kBar, 2 * kBar);
  for (int64_t t : {(int64_t)0, (int64_t)kBar, (int64_t)(2 * kBar - 1)})
    CHECK_NEAR(bpmAt(d, t), bpmAt(original, t), 1e-6);              // the music before the cut is untouched
  CHECK_NEAR(bpmAt(d, kBar), 150, 1e-6);                            // before the fix: 140
  for (int64_t t : {(int64_t)(2 * kBar), (int64_t)(3 * kBar), (int64_t)(4 * kBar - 1)})
    CHECK_NEAR(bpmAt(d, t), 180, 1e-9);                             // the empty bars hold the cut's tempo
  for (int64_t t : {(int64_t)(4 * kBar), (int64_t)(5 * kBar), (int64_t)(6 * kBar)})
    CHECK_NEAR(bpmAt(d, t), bpmAt(original, t - 2 * kBar), 1e-6);   // the moved music keeps its curve (210 at 5 bars)
  CHECK_NEAR(engineSeconds(d, kBar), engineSeconds(original, kBar), 1e-4);
  CHECK_NEAR(engineSeconds(d, 6 * kBar) - engineSeconds(d, 4 * kBar),
             engineSeconds(original, 4 * kBar) - engineSeconds(original, 2 * kBar), 1e-4);
  CHECK(validSavedForm(d));
  // Inverse edit: deleting the same bars restores the music's tempo (Undo is the byte-exact restore path;
  // the boundary pins the insert added remain, so the JSON is not byte-identical).
  deleteTime(d, 2 * kBar, 2 * kBar);
  CHECK_EQ(d.lengthTicks, original.lengthTicks);
  for (int64_t t : {(int64_t)0, (int64_t)kBar, (int64_t)(2 * kBar), (int64_t)(3 * kBar), (int64_t)(4 * kBar)})
    CHECK_NEAR(bpmAt(d, t), bpmAt(original, t), 1e-6);              // before the fix: 140 at bar 1
  CHECK(validSavedForm(d));

  // Step control: no interpolation, the earlier music keeps its tempo (this already held).
  Document s = newEmptyDocument();
  s.bpm = 120;
  setTempoMap(s, {{4 * kBar, 240, json::object(), false}});
  insertTime(s, 2 * kBar, 2 * kBar);
  CHECK_NEAR(bpmAt(s, kBar), 120, 1e-9);
  CHECK_NEAR(bpmAt(s, 3 * kBar), 120, 1e-9);
  CHECK_NEAR(bpmAt(s, 6 * kBar), 240, 1e-9);
  CHECK(validSavedForm(s));

  // A step exactly at the cut: the hold keeps the new value, and the inverse edit stays a valid document.
  Document c = newEmptyDocument();
  c.bpm = 120;
  setTempoMap(c, {{2 * kBar, 200, json::object(), false}});
  const Document cOrig = c;
  insertTime(c, 2 * kBar, 2 * kBar);
  CHECK_NEAR(bpmAt(c, kBar), 120, 1e-9);
  CHECK_NEAR(bpmAt(c, 3 * kBar), 200, 1e-9);                        // before the fix: 120 (the step moved away)
  CHECK_NEAR(bpmAt(c, 4 * kBar), 200, 1e-9);
  CHECK(validSavedForm(c));
  deleteTime(c, 2 * kBar, 2 * kBar);
  CHECK(validSavedForm(c));                                         // no two changes on one tick
  CHECK_NEAR(bpmAt(c, 2 * kBar), 200, 1e-9);
  CHECK_EQ(toJson(c).dump(), toJson(cOrig).dump());

  // Inserting at the very start: the empty bars hold the song's start tempo, the ramp resumes after them.
  Document z = newEmptyDocument();
  z.bpm = 120;
  setTempoMap(z, {{4 * kBar, 240, json::object(), true}});
  const Document zOrig = z;
  insertTime(z, 0, 2 * kBar);
  CHECK_NEAR(bpmAt(z, kBar), 120, 1e-9);                            // before the fix: 140
  CHECK_NEAR(bpmAt(z, 3 * kBar), bpmAt(zOrig, kBar), 1e-6);
  CHECK_NEAR(bpmAt(z, 6 * kBar), 240, 1e-6);
  CHECK_NEAR(engineSeconds(z, 6 * kBar) - engineSeconds(z, 2 * kBar), engineSeconds(zOrig, 4 * kBar), 1e-4);
  CHECK(validSavedForm(z));

  // The 64-point cap: the boundary pins count, and a map over the cap throws with the song untouched.
  Document full = newEmptyDocument();
  full.bpm = 120;
  full.lengthTicks = 100 * kBar;
  std::vector<TempoPoint> many;
  for (int i = 1; i <= 64; ++i) many.push_back({(int64_t)i * kBar, 120.0 + (i % 5), json::object(), i == 33});
  setTempoMap(full, many);
  CHECK_THROWS(insertTime(full, 32 * kBar + kBar / 2, kBar));
  CHECK_EQ(full.tempoMap.size(), (size_t)64);
  CHECK_EQ(full.lengthTicks, 100 * kBar);
}

TEST("BUG-SWEEP-25: shortening the song keeps the retained ramp with a derived endpoint") {
  using namespace tempo_sweep;
  // The probe's song: removing the future ramp target used to flatten the retained passage (150 -> 120,
  // 1.78515 s -> 2 s). The documented past-end removal stays; the glide gets an in-range endpoint.
  Document d = newEmptyDocument();
  d.bpm = 120;
  setTempoMap(d, {{4 * kBar, 240, json::object(), true}});
  const Document original = d;
  setSongLength(d, 2 * kBar);
  CHECK_EQ(d.lengthTicks, 2 * kBar);
  CHECK_NEAR(bpmAt(d, kBar), 150, 1e-6);                            // before the fix: 120
  CHECK_NEAR(bpmAt(d, 2 * kBar - 1), bpmAt(original, 2 * kBar - 1), 1e-6);
  CHECK_NEAR(engineSeconds(d, kBar), engineSeconds(original, kBar), 1e-4);
  CHECK_EQ(d.tempoMap.size(), (size_t)1);                           // the derived endpoint, inside the song
  if (!d.tempoMap.empty()) {
    CHECK_EQ(d.tempoMap[0].tick, 2 * kBar - 1);
    CHECK(d.tempoMap[0].ramp);
  }
  CHECK(validSavedForm(d));
  // Extending again does not resurrect the removed target (documented destructive removal; Undo is the
  // restore path), but the retained passage keeps playing the ramp it had.
  setSongLength(d, original.lengthTicks);
  CHECK_NEAR(bpmAt(d, kBar), 150, 1e-6);                            // before the fix: 120
  CHECK(validSavedForm(d));

  // A glide between two kept points, heading for a removed target: the retained shape survives.
  Document g = newEmptyDocument();
  g.bpm = 120;
  setTempoMap(g, {{1 * kBar, 180, json::object(), true}, {4 * kBar, 240, json::object(), true}});
  const Document gOrig = g;
  setSongLength(g, 3 * kBar);
  CHECK_NEAR(bpmAt(g, 2 * kBar), bpmAt(gOrig, 2 * kBar), 1e-6);     // before the fix: 180, the ramp flattened
  CHECK_NEAR(bpmAt(g, 3 * kBar - 1), bpmAt(gOrig, 3 * kBar - 1), 1e-6);
  CHECK_EQ(g.tempoMap.size(), (size_t)2);
  CHECK(validSavedForm(g));

  // Shortening onto the target's own tick: it goes (documented), the retained glide keeps its shape.
  Document t = newEmptyDocument();
  t.bpm = 120;
  setTempoMap(t, {{4 * kBar, 240, json::object(), true}});
  const Document tOrig = t;
  setSongLength(t, 4 * kBar);
  CHECK_NEAR(bpmAt(t, 2 * kBar), bpmAt(tOrig, 2 * kBar), 1e-6);
  CHECK_EQ(t.tempoMap.size(), (size_t)1);
  CHECK(validSavedForm(t));

  // Step control: a step past the new end goes, as documented, and nothing else changes.
  Document s = newEmptyDocument();
  s.bpm = 120;
  setTempoMap(s, {{4 * kBar, 240, json::object(), false}});
  setSongLength(s, 2 * kBar);
  CHECK(s.tempoMap.empty());
  CHECK_NEAR(bpmAt(s, kBar), 120, 1e-9);

  // Extending only (the other callers: stutter, pattern rack, the UI) never touches the map.
  Document e = newEmptyDocument();
  e.bpm = 120;
  setTempoMap(e, {{4 * kBar, 240, json::object(), true}});
  const json mapBefore = toJson(e)["transport"]["tempoMap"];
  setSongLength(e, 20 * kBar);
  CHECK_EQ(toJson(e)["transport"]["tempoMap"].dump(), mapBefore.dump());
}

// ---------------------------------------------------------------- BUG-SWEEP-26 (2026-11-18)
// applyRackTemplate wrote the template's notes without the slot's swing: a slot at swing 0.5 played the loaded
// template straight, and the next setRackSwing was a no-op (its stale "from" put the notes off the 1/16 grid,
// which swingNote refuses). rackWriteShape already re-applies the slot swing after writing; Load Template must too.

TEST("sweep-26: a template loaded into a swung slot plays swung and the next swing change works") {
  Document d = newEmptyDocument();
  const std::string bass = addTrack(d, "bass");
  setRackSwing(d, "s1", 0.5);  // the default slot, saved with swing 0.5
  PatternTemplate t;
  t.id = "tpl";
  t.name = "Tpl";
  t.bars = 4;
  PatternTemplateRow row;
  row.part = "bass";
  row.name = "Bass line";
  row.lengthTicks = 4 * kBar;
  Note n;
  n.tick = 240;  // an odd 1/16: at swing 0.5 it plays at 240 + 60
  n.length = 120;
  n.pitch = 36;
  n.velocity = 0.8;
  row.notes.push_back(n);
  t.rows.push_back(row);
  CHECK_EQ(applyRackTemplate(d, nullptr, t, "s1"), 1);
  const auto slots = rackSlots(d);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* p = rackPattern(d, *slot, bass);
  CHECK(p != nullptr);
  if (!p) return;
  CHECK_EQ(p->notes.size(), (size_t)1);
  if (p->notes.empty()) return;
  CHECK_EQ(p->notes[0].tick, (int64_t)300);  // before: 240, the slot's swing was not applied to the loaded notes
  CHECK(p->notes[0].extra.contains("rackSwingBase"));
  setRackSwing(d, "s1", 0.75);
  const auto after = rackSlots(d);
  const PatternSlot* slot2 = findRackSlot(after, "s1");
  CHECK(slot2 != nullptr);
  if (!slot2) return;
  const Pattern* q = rackPattern(d, *slot2, bass);
  CHECK(q != nullptr);
  if (!q || q->notes.empty()) return;
  CHECK_EQ(q->notes[0].tick, (int64_t)330);  // before: 240, the stale from=60 made swingNote skip the note
}

TEST("sweep-26 control: swing back to zero then 0.75 lands at 330") {
  Document d = newEmptyDocument();
  const std::string bass = addTrack(d, "bass");
  setRackSwing(d, "s1", 0.5);
  PatternTemplate t;
  t.id = "tpl";
  t.name = "Tpl";
  t.bars = 4;
  PatternTemplateRow row;
  row.part = "bass";
  row.name = "Bass line";
  row.lengthTicks = 4 * kBar;
  Note n;
  n.tick = 240;
  n.length = 120;
  n.pitch = 36;
  row.notes.push_back(n);
  t.rows.push_back(row);
  CHECK_EQ(applyRackTemplate(d, nullptr, t, "s1"), 1);
  setRackSwing(d, "s1", 0.0);
  const auto zeroed = rackSlots(d);
  const PatternSlot* slot = findRackSlot(zeroed, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* p = rackPattern(d, *slot, bass);
  CHECK(p != nullptr);
  if (!p || p->notes.empty()) return;
  CHECK_EQ(p->notes[0].tick, (int64_t)240);  // the unswung position, the swing record gone
  CHECK(!p->notes[0].extra.contains("rackSwingBase"));
  setRackSwing(d, "s1", 0.75);
  const auto swung = rackSlots(d);
  const PatternSlot* slot2 = findRackSlot(swung, "s1");
  CHECK(slot2 != nullptr);
  if (!slot2) return;
  const Pattern* q = rackPattern(d, *slot2, bass);
  CHECK(q != nullptr);
  if (!q || q->notes.empty()) return;
  CHECK_EQ(q->notes[0].tick, (int64_t)330);  // 240 + swingTicks(0.75) = 240 + 90
}

TEST("sweep-26 builtin: a P-Core style loaded into a swung slot keeps the swing") {
  const fs::path catalogue = fs::path(__FILE__).parent_path().parent_path() / "pcore" / "styles.json";
  const PCoreCatalog c = loadPCoreCatalog(catalogue);
  const PCoreStyle* style = c.style("fullon");
  CHECK(style != nullptr);
  if (!style) return;
  const int64_t jitter = (int64_t)std::lround(style->humanize * 24);  // the style's humanize, as rackWriteShape uses
  Document d = newEmptyDocument();
  const std::string bass = addTrack(d, "bass");
  setRackSwing(d, "s1", 0.5);
  PatternTemplate t;
  t.id = "style-fullon";
  t.name = "Full-on";
  t.style = "fullon";
  t.bars = 4;
  t.builtin = true;
  CHECK_EQ(applyRackTemplate(d, &c, t, "s1", 7), 1);
  const auto slots = rackSlots(d);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* p = rackPattern(d, *slot, bass);
  CHECK(p != nullptr);
  if (!p) return;
  CHECK(!p->notes.empty());
  bool swung = false;
  for (const auto& note : p->notes) {
    const int64_t base = note.tick - 60;  // swingTicks(0.5)
    const int64_t grid = (base + 120) / 240 * 240;
    const int64_t diff = base > grid ? base - grid : grid - base;
    if (grid > 0 && (grid / 240) % 2 == 1 && diff <= jitter && note.extra.contains("rackSwingBase")) swung = true;
  }
  CHECK(swung);  // before: the style wrote straight notes, none delayed by the slot's swing
}

// ---------------------------------------------------------------- BUG-SWEEP-27 (2026-11-18)
// parseDocument keeps extra.patternSlots unvalidated on purpose (compat: document.cpp r.rest()), but slotFromJson
// read it with typed j.value("bars", 4) / j.value("swing", 0.0): a wrong type threw type_error.302 out of rackSlots,
// the consumption boundary, with no catch anywhere in the UI chain. The consumer must recover with defaults.

TEST("sweep-27: malformed patternSlots metadata is read with defaults, not thrown at the consumer") {
  Document d = newEmptyDocument();
  addTrack(d, "bass");
  json j = toJson(d);
  j["patternSlots"] = json::array({
      {{"id", "s1"}, {"bars", "four"}, {"swing", "lots"}, {"name", 7}, {"root", json::array()}, {"scale", 3}, {"tracks", "nope"}},
      {{"id", 42}, {"bars", 4}},                      // id not a string: the entry is skipped, not a throw
      "not an object",                                // already skipped before the fix
      {{"id", "s2"}, {"bars", 2}, {"swing", 0.5}}});  // a well-formed neighbour survives
  Document parsed = parseDocument(j);  // the parser keeps patternSlots as extra, unvalidated, by design
  CHECK_EQ(parsed.extra["patternSlots"].size(), (size_t)4);
  bool threw = false;
  std::vector<PatternSlot> slots;
  try {
    slots = rackSlots(parsed);
  } catch (const std::exception& e) {
    threw = true;
    std::fprintf(stderr, "  rackSlots threw: %s\n", e.what());
  }
  CHECK(!threw);  // before: type_error.302 from j.value("bars", 4) on the string "four"
  if (threw) return;
  const PatternSlot* s1 = findRackSlot(slots, "s1");
  CHECK(s1 != nullptr);
  if (s1) {
    CHECK_EQ(s1->bars, 4);  // conservative default for a bars that is not a number
    CHECK_NEAR(s1->swing, 0.0, 1e-12);
    CHECK_EQ(s1->name, std::string("Pattern 1"));  // a name that is not a string falls back to the default naming
    CHECK(s1->root.empty());
    CHECK(s1->scale.empty());
    CHECK(s1->tracks.empty());
  }
  CHECK(findRackSlot(slots, "s2") != nullptr);
  const PatternSlot* s2 = findRackSlot(slots, "s2");
  if (s2) {
    CHECK_EQ(s2->bars, 2);
    CHECK_NEAR(s2->swing, 0.5, 1e-12);
  }
  CHECK_EQ(slots.size(), (size_t)2);  // the numeric-id entry and the non-object entry are left out
}

TEST("sweep-27: unknown song and slot properties survive the parse and save round-trip") {
  Document d = newEmptyDocument();
  json j = toJson(d);
  j["futureFeature"] = {{"mode", "x"}};
  j["patternSlots"] = json::array({{{"id", "s1"}, {"bars", 4}, {"mystery", "keep-me"}}});
  Document parsed = parseDocument(j);
  CHECK(parsed.extra.contains("futureFeature"));
  CHECK_EQ(parsed.extra["patternSlots"][0]["mystery"], std::string("keep-me"));
  const json out = toJson(parsed);
  CHECK(out.contains("futureFeature"));
  CHECK_EQ(out["patternSlots"][0]["mystery"], std::string("keep-me"));
  const Document again = parseDocument(out);
  CHECK_EQ(again.extra["patternSlots"][0]["mystery"], std::string("keep-me"));
}

// ---------------------------------------------------------------- BUG-SWEEP-28 (2026-11-18)
// pattern_rack.hpp's Load Template contract says "the same part first (else the same name)", and matchRows' own
// comment agrees, but the code ran the NAME pass first: with conflicting names/parts the first pick stole the row
// another track needed (the used set), mapping notes onto the wrong musical parts. Part first, name second.

TEST("sweep-28: conflicting names and parts map by part first, as the header contract says") {
  Document d = newEmptyDocument();
  const std::string bass = addTrack(d, "bass", "Bass");
  const std::string lead = addTrack(d, "lead", "Lead");
  PatternTemplate t;
  t.id = "conflict";
  t.name = "Conflict";
  t.bars = 1;
  PatternTemplateRow rb;  // the bass row, named "Lead"
  rb.part = "bass";
  rb.name = "Lead";
  rb.lengthTicks = kBar;
  Note nb;
  nb.tick = 0;
  nb.length = 120;
  nb.pitch = 36;
  rb.notes.push_back(nb);
  PatternTemplateRow rl;  // the lead row, named "Bass"
  rl.part = "lead";
  rl.name = "Bass";
  rl.lengthTicks = kBar;
  Note nl;
  nl.tick = 0;
  nl.length = 120;
  nl.pitch = 84;
  rl.notes.push_back(nl);
  t.rows.push_back(rb);
  t.rows.push_back(rl);
  CHECK_EQ(applyRackTemplate(d, nullptr, t, "s1"), 2);
  const auto slots = rackSlots(d);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* pb = rackPattern(d, *slot, bass);
  const Pattern* pl = rackPattern(d, *slot, lead);
  CHECK(pb != nullptr);
  CHECK(pl != nullptr);
  if (!pb || !pl || pb->notes.empty() || pl->notes.empty()) return;
  CHECK_EQ(pb->notes[0].pitch, 36);  // before: 84, the name "Bass" stole the lead row
  CHECK_EQ(pl->notes[0].pitch, 84);  // before: 36, the name "Lead" stole the bass row
}

TEST("sweep-28: one row of a part goes to the first track of that part only") {
  Document d = newEmptyDocument();
  const std::string b1 = addTrack(d, "bass", "Bass");
  const std::string b2 = addTrack(d, "bass", "Sub Bass");  // a second bass track, id bass-2
  PatternTemplate t;
  t.id = "one-bass";
  t.name = "One bass";
  t.bars = 1;
  PatternTemplateRow row;
  row.part = "bass";
  row.name = "Whatever";
  row.lengthTicks = kBar;
  Note n;
  n.tick = 0;
  n.length = 120;
  n.pitch = 36;
  row.notes.push_back(n);
  t.rows.push_back(row);
  CHECK_EQ(rackTemplateTracks(d, t), 1);
  CHECK_EQ(applyRackTemplate(d, nullptr, t, "s1"), 1);
  const auto slots = rackSlots(d);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* p1 = rackPattern(d, *slot, b1);
  CHECK(p1 != nullptr);
  if (p1) {
    CHECK_EQ(p1->notes.size(), (size_t)1);
    if (!p1->notes.empty()) CHECK_EQ(p1->notes[0].pitch, 36);
  }
  const Pattern* p2 = rackPattern(d, *slot, b2);  // the row was used: the second bass stays empty
  CHECK(p2 != nullptr);
  if (p2) CHECK(p2->notes.empty());
}

TEST("sweep-28: unambiguous name-only and part-only matches keep working") {
  Document d = newEmptyDocument();
  const std::string keys = addTrack(d, "keys", "Rhodes");  // no part guessed from id/name
  const std::string kick = addTrack(d, "kick", "Kick");
  CHECK(pcoreGuessPart(d.tracks[0]).empty());
  PatternTemplate t;
  t.id = "unambiguous";
  t.name = "Unambiguous";
  t.bars = 1;
  PatternTemplateRow byName;
  byName.part = "zzz-unknown";
  byName.name = "Rhodes";
  byName.lengthTicks = kBar;
  Note nk;
  nk.tick = 0;
  nk.length = 120;
  nk.pitch = 60;
  byName.notes.push_back(nk);
  PatternTemplateRow byPart;
  byPart.part = "kick";
  byPart.name = "Totally Different";
  byPart.lengthTicks = kBar;
  Note nkk;
  nkk.tick = 0;
  nkk.length = 120;
  nkk.pitch = 36;
  byPart.notes.push_back(nkk);
  t.rows.push_back(byName);
  t.rows.push_back(byPart);
  CHECK_EQ(rackTemplateTracks(d, t), 2);
  CHECK_EQ(applyRackTemplate(d, nullptr, t, "s1"), 2);
  const auto slots = rackSlots(d);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* pk = rackPattern(d, *slot, keys);
  const Pattern* pd = rackPattern(d, *slot, kick);
  CHECK(pk != nullptr);
  CHECK(pd != nullptr);
  if (!pk || !pd || pk->notes.empty() || pd->notes.empty()) return;
  CHECK_EQ(pk->notes[0].pitch, 60);  // name-only: the part is unknown on both sides
  CHECK_EQ(pd->notes[0].pitch, 36);  // part-only: the names do not match
}

// ---------------------------------------------------------------- BUG-SWEEP-20 (2026-11-18)
// Save as Template stripped hasRoll, and the template schema had no roll at all (tick/length/pitch/velocity
// only): a reusable pattern saved from a slot with active rolls came back plain. The roll now travels with
// the template note as an OPTIONAL "roll" object; absent means no roll, so every template written before
// this keeps loading and saving unchanged (the catalogue preservation rule).

TEST("sweep-20: a slot saved as a template keeps its rolls through save, load and apply") {
  Document d = newEmptyDocument();
  const std::string snare = addTrack(d, "snare");
  Pattern& p = rackPatternFor(d, "s1", snare);
  Note plain; plain.id = "n1"; plain.tick = 0; plain.length = 240; plain.pitch = 38; plain.velocity = 0.9;
  Note rolled; rolled.id = "n2"; rolled.tick = 480; rolled.length = 480; rolled.pitch = 38; rolled.velocity = 0.5;
  rolled.hasRoll = true; rolled.roll.count = 4; rolled.roll.velocityFrom = 0.25; rolled.roll.velocityTo = 1.0;
  p.notes = {plain, rolled};
  const PatternTemplate t = rackTemplateFromSlot(d, "s1", "Rolled groove");
  CHECK_EQ(t.rows.size(), (size_t)1);
  if (t.rows.empty()) return;
  CHECK_EQ(t.rows[0].notes.size(), (size_t)2);
  if (t.rows[0].notes.size() != 2) return;
  const Note& saved = t.rows[0].notes[1];
  CHECK(saved.hasRoll);  // before: the strip cleared it
  CHECK_EQ(saved.roll.count, 4);
  CHECK_NEAR(saved.roll.velocityFrom, 0.25, 1e-9);
  CHECK_NEAR(saved.roll.velocityTo, 1.0, 1e-9);
  const fs::path dir = freshDir("sweep20-templates");
  saveRackTemplate(dir, t);
  std::vector<std::string> problems;
  const auto loaded = loadRackTemplates(dir, &problems);
  CHECK(problems.empty());
  CHECK_EQ(loaded.size(), (size_t)1);
  if (loaded.size() != 1 || loaded[0].rows.empty() || loaded[0].rows[0].notes.size() != 2) return;
  const Note& read = loaded[0].rows[0].notes[1];
  CHECK(read.hasRoll);  // before: the schema had no roll, the reader could not return one
  CHECK_EQ(read.roll.count, 4);
  CHECK_NEAR(read.roll.velocityFrom, 0.25, 1e-9);
  CHECK_NEAR(read.roll.velocityTo, 1.0, 1e-9);
  CHECK(!loaded[0].rows[0].notes[0].hasRoll);  // the plain note stays plain
  Document e = newEmptyDocument();
  const std::string snare2 = addTrack(e, "snare");
  CHECK_EQ(applyRackTemplate(e, nullptr, loaded[0], "s1"), 1);
  const auto slots = rackSlots(e);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* q = rackPattern(e, *slot, snare2);
  CHECK(q != nullptr);
  if (!q || q->notes.size() != 2) return;
  CHECK(q->notes[1].hasRoll);  // before: the applied pattern came back plain
  CHECK_EQ(q->notes[1].roll.count, 4);
  CHECK_NEAR(q->notes[1].roll.velocityFrom, 0.25, 1e-9);
  CHECK_NEAR(q->notes[1].roll.velocityTo, 1.0, 1e-9);
  CHECK(!q->notes[0].hasRoll);
}

TEST("sweep-20: templates written before rolls keep loading and saving unchanged") {
  // Exactly what the old writer produced: tick/length/pitch/velocity only, no roll anywhere.
  const std::string legacy =
      "{\"format\":\"core-patterns\",\"version\":1,\"id\":\"old\",\"name\":\"Old groove\",\"bars\":2,"
      "\"rows\":[{\"part\":\"kick\",\"name\":\"Kick\",\"lengthTicks\":7680,\"notes\":["
      "{\"tick\":0,\"length\":240,\"pitch\":36,\"velocity\":0.9},"
      "{\"tick\":960,\"length\":240,\"pitch\":36,\"velocity\":0.7}]}]}";
  const PatternTemplate t = rackTemplateFromJson(json::parse(legacy));
  CHECK_EQ(t.rows.size(), (size_t)1);
  if (t.rows.empty()) return;
  CHECK_EQ(t.rows[0].notes.size(), (size_t)2);
  for (const auto& n : t.rows[0].notes) {
    CHECK(!n.hasRoll);  // absent roll = no roll
    CHECK_EQ(n.roll.count, 0);
  }
  // The writer adds nothing to a roll-less template: the saved form is the old form, key for key.
  CHECK(rackTemplateToJson(t) == json::parse(legacy));
  const fs::path dir = freshDir("sweep20-legacy");
  writeText(dir / ("old" + std::string(kRackTemplateExtension)), legacy);
  std::vector<std::string> problems;
  const auto loaded = loadRackTemplates(dir, &problems);
  CHECK(problems.empty());
  CHECK_EQ(loaded.size(), (size_t)1);
  if (loaded.empty()) return;
  const fs::path saved = saveRackTemplate(dir, loaded[0]);  // a second copy, id old-2
  json expected = json::parse(legacy);
  expected["id"] = "old-2";  // saveRackTemplate keeps the existing file and takes the next free id
  CHECK(json::parse(bytesIn(saved)) == expected);  // the old form, key for key, nothing added
  CHECK(bytesIn(saved).find("roll") == std::string::npos);  // no roll key written for roll-less notes
  Document e = newEmptyDocument();
  const std::string kick = addTrack(e, "kick");
  CHECK_EQ(applyRackTemplate(e, nullptr, loaded[0], "s1"), 1);
  const auto slots = rackSlots(e);
  const PatternSlot* slot = findRackSlot(slots, "s1");
  CHECK(slot != nullptr);
  if (!slot) return;
  const Pattern* q = rackPattern(e, *slot, kick);
  CHECK(q != nullptr);
  if (!q) return;
  CHECK_EQ(q->notes.size(), (size_t)2);
  for (const auto& n : q->notes) CHECK(!n.hasRoll);
}

// BUG-SWEEP-16 (work preservation): the preset/template/roles writers answered success with an empty file
// when the write failed late (after the open, at the flush/close), over what the user had. Each case here
// failed before the fix: no exception, and an empty file on disk.
TEST("write safety BUG-SWEEP-16: a late write failure on a pattern preset throws and leaves no empty file") {
  const fs::path dir = freshDir("ws16-preset");
  PatternPreset first;
  first.id = "mine/first";
  first.name = "First";
  first.lengthTicks = kBar;
  Note n;
  n.id = "n1"; n.tick = 0; n.length = 480; n.pitch = 36; n.velocity = 1.0;
  first.notes.push_back(n);
  const PatternPreset saved = savePatternPreset(dir, first, {});
  const std::string before = bytesIn(saved.file);
  CHECK(!before.empty());
  PatternPreset next = first;
  next.id = "mine/second";
  next.name = "Second";
  bool threw = false;
  {
    WriteFaultAgain restore;
    setWriteFaultForTests(lateFlushFault);
    try { savePatternPreset(dir, next, {saved}); } catch (const DocumentError&) { threw = true; }
  }
  CHECK(threw);  // before: success, and an empty .json left in the library
  CHECK_EQ(bytesIn(saved.file), before);  // the preset already in the folder, byte for byte
  CHECK(!fs::exists(dir / "mine" / "second.json"));  // no empty file at the new path
  CHECK(noEmptyFileUnder(dir));
  std::vector<std::string> problems;
  CHECK_EQ(loadPatternLibrary({{"user", dir}}, &problems).size(), (size_t)1);
  CHECK(problems.empty());
  // The normal path still works afterwards: the new preset is written and reads back.
  const PatternPreset ok = savePatternPreset(dir, next, {saved});
  CHECK(fs::file_size(ok.file) > 0);
  CHECK_EQ(loadPatternLibrary({{"user", dir}}, &problems).size(), (size_t)2);
  CHECK(problems.empty());
}

TEST("write safety BUG-SWEEP-16: a late write failure on a rack template throws and leaves no empty file") {
  const fs::path dir = freshDir("ws16-template");
  PatternTemplate keep;
  keep.id = "keep";
  keep.name = "Keep";
  keep.bpm = 140;
  keep.bars = 2;
  PatternTemplateRow row;
  row.part = "kick";
  row.name = "Kick";
  row.lengthTicks = 2 * kBar;
  Note n;
  n.tick = 0; n.length = 240; n.pitch = 36; n.velocity = 0.9;
  row.notes.push_back(n);
  keep.rows.push_back(row);
  const fs::path keepFile = saveRackTemplate(dir, keep);
  const std::string before = bytesIn(keepFile);
  CHECK(!before.empty());
  PatternTemplate t = keep;
  t.id = "mine";
  t.name = "Mine";
  bool threw = false;
  {
    WriteFaultAgain restore;
    setWriteFaultForTests(lateFlushFault);
    try { saveRackTemplate(dir, t); } catch (const DocumentError&) { threw = true; }
  }
  CHECK(threw);  // before: success, with an empty template file
  CHECK_EQ(bytesIn(keepFile), before);  // the template already in the folder, byte for byte
  CHECK(!fs::exists(dir / ("mine" + std::string(kRackTemplateExtension))));  // no empty file at the new path
  CHECK(noEmptyFileUnder(dir));
  std::vector<std::string> problems;
  CHECK_EQ(loadRackTemplates(dir, &problems).size(), (size_t)1);
  CHECK(problems.empty());
  // The normal path still works afterwards.
  const fs::path ok = saveRackTemplate(dir, t);
  CHECK(fs::file_size(ok) > 0);
  CHECK_EQ(loadRackTemplates(dir, &problems).size(), (size_t)2);
  CHECK(problems.empty());
}

TEST("write safety BUG-SWEEP-16: a late write failure on the roles file throws and the previous map stays byte-identical") {
  const fs::path dir = freshDir("ws16-roles");
  const fs::path file = dir / "roles.json";
  const json plugin = {{"format", "vst3"}, {"id", "VST3-Room-1234"}, {"name", "Room"}, {"file", "C:/VST3/Room.vst3"}};
  RoleMap first;
  first.roles["reverb"] = {plugin, {{"mix", "0"}}, {}};
  saveRoleMap(first, file);
  const std::string before = bytesIn(file);
  CHECK(!before.empty());
  RoleMap next = first;
  next.roles["delay"] = {plugin, {{"mix", "3"}}, {}};
  bool threw = false;
  {
    WriteFaultAgain restore;
    setWriteFaultForTests(lateFlushFault);
    try { saveRoleMap(next, file); } catch (const DocumentError&) { threw = true; }
  }
  CHECK(threw);  // before: success, and the roles file emptied
  CHECK_EQ(bytesIn(file), before);  // the user's roles, byte for byte
  CHECK(!fs::exists(fs::path(file.string() + ".tmp")));
  CHECK(noEmptyFileUnder(dir));
  std::string problem;
  CHECK_EQ(loadRoleMap(file, &problem).roles.size(), (size_t)1);  // still reads as it was
  CHECK(problem.empty());
  // A successful save afterwards keeps the previous map as .bak (before: it was overwritten, no backup).
  saveRoleMap(next, file);
  CHECK_EQ(loadRoleMap(file).roles.size(), (size_t)2);
  CHECK_EQ(bytesIn(backupPath(file)), before);
  CHECK(noEmptyFileUnder(dir));
}

// ---------------------------------------------------------------- BUG-SWEEP-21 (ratified 2026-11-18)
// Owner, verbatim: «split em clip congelado = recusa com mensagem + oferta de unfreeze (um Undo); nunca meio-congelado.»
// A frozen clip (freezeSeconds > 0) holds a piece of the file from offsetSeconds for its whole length
// (engine clip_grains.hpp). The old split re-anchored the second piece's offset (offsetSeconds += cut) and
// kept freezeSeconds: the second piece held a DIFFERENT piece of the file — a half-frozen state. The split
// is refused with an unfreeze offer; unfreeze (one Undo) then split plays exactly the unsplit file.

namespace {
// One audio track with one clip (no file needed: the split is document math). 150 bpm, so tick 480 = 0.2 s.
Document clipSong(double freezeSeconds, double offsetSeconds = 0.1, double duration = 1.0) {
  Document d = newEmptyDocument();
  d.bpm = 150;
  const std::string tid = addAudioTrack(d, "take");
  AudioClip c;
  c.id = "x1";
  c.assetId = "a1";
  c.startTick = 0;
  c.offsetSeconds = offsetSeconds;
  c.durationSeconds = duration;
  c.fadeInSeconds = 0.02;
  c.fadeOutSeconds = 0.03;
  c.freezeSeconds = freezeSeconds;
  trackById(d, tid).audioClips.push_back(c);
  return d;
}
bool mentionsUnfreeze(const std::string& message) {
  std::string lower;
  for (char ch : message) lower += (char)std::tolower((unsigned char)ch);
  return lower.find("unfreeze") != std::string::npos;
}
}  // namespace

TEST("freeze split BUG-SWEEP-21: a frozen clip refuses the split with an unfreeze offer and stays whole") {
  Document d = clipSong(0.05);
  const std::string tid = d.tracks.back().id;
  const AudioClip before = trackById(d, tid).audioClips[0];
  bool refused = false;
  std::string message;
  try { (void)splitAudioClip(d, tid, "x1", 480); } catch (const DocumentError& e) { refused = true; message = e.what(); }
  CHECK(refused);  // before: the split went through, the second piece holding a different part of the file
  CHECK(mentionsUnfreeze(message));  // the refusal carries the offer
  const auto& clips = trackById(d, tid).audioClips;
  CHECK_EQ(clips.size(), (size_t)1);  // never half-frozen: no second piece
  CHECK_NEAR(clips[0].freezeSeconds, before.freezeSeconds, 0);
  CHECK_NEAR(clips[0].offsetSeconds, before.offsetSeconds, 0);
  CHECK_NEAR(clips[0].durationSeconds, before.durationSeconds, 0);
  CHECK_NEAR(clips[0].fadeInSeconds, before.fadeInSeconds, 0);
  CHECK_NEAR(clips[0].fadeOutSeconds, before.fadeOutSeconds, 0);
  // Every split-based cut refuses the same way: slice and range removal never leave half-frozen pieces.
  Document s = clipSong(0.05);
  CHECK_THROWS(sliceAudioClip(s, tid, "x1", 240));
  CHECK_EQ(trackById(s, tid).audioClips.size(), (size_t)1);
  CHECK_NEAR(trackById(s, tid).audioClips[0].freezeSeconds, 0.05, 0);
  Document r = clipSong(0.05);
  CHECK_THROWS(removeAudioRange(r, tid, "x1", 240, 720));
  CHECK_EQ(trackById(r, tid).audioClips.size(), (size_t)1);
  CHECK_NEAR(trackById(r, tid).audioClips[0].freezeSeconds, 0.05, 0);
  // Control: an unfrozen clip splits normally (the refusal is only for the frozen hold).
  Document u = clipSong(0);
  const std::string second = splitAudioClip(u, tid, "x1", 480);
  CHECK(!second.empty());
  CHECK_EQ(trackById(u, tid).audioClips.size(), (size_t)2);
}

TEST("freeze split BUG-SWEEP-21: unfreeze is one Undo step, then the split succeeds normally") {
  Document d = clipSong(0.05);
  const std::string tid = d.tracks.back().id;
  History h(d);
  auto held = [&]() -> const AudioClip& { return h.current().findTrack(tid)->audioClips[0]; };
  Document next = h.current();
  unfreezeAudioClip(next, tid, "x1");
  h.commit(next, "Unfreeze audio clip");
  CHECK_NEAR(held().freezeSeconds, 0.0, 0);
  CHECK(h.canUndo());
  CHECK_EQ(h.undoLabel(), std::string("Unfreeze audio clip"));
  CHECK(h.undo());  // one Undo brings the frozen hold back, whole
  CHECK_NEAR(held().freezeSeconds, 0.05, 0);
  CHECK_EQ(h.current().findTrack(tid)->audioClips.size(), (size_t)1);
  CHECK(h.redo());
  // Unfrozen, the split works normally: two plain pieces that play the file sample for sample
  // (the null proof is engine_tests' "freeze split BUG-SWEEP-21").
  Document s = h.current();
  const std::string second = splitAudioClip(s, tid, "x1", 480);
  CHECK(!second.empty());
  const auto& clips = trackById(s, tid).audioClips;
  CHECK_EQ(clips.size(), (size_t)2);
  CHECK_NEAR(clips[0].durationSeconds, 0.2, 1e-9);
  CHECK_NEAR(clips[0].freezeSeconds, 0.0, 0);
  CHECK_EQ(clips[1].id, second);
  CHECK_EQ(clips[1].startTick, (int64_t)480);
  CHECK_NEAR(clips[1].offsetSeconds, 0.3, 1e-9);  // 0.1 + 0.2: the file continues where the first piece stopped
  CHECK_NEAR(clips[1].durationSeconds, 0.8, 1e-9);
  CHECK_NEAR(clips[1].freezeSeconds, 0.0, 0);
  // Unfreeze of a plain clip alters nothing: History adds no step for it.
  History p(clipSong(0));
  const std::string ptid = p.current().tracks.back().id;
  Document q = p.current();
  unfreezeAudioClip(q, ptid, "x1");
  p.commit(q, "Unfreeze audio clip");
  CHECK(!p.canUndo());
}

// ---------------------------------------------------------------- BUG-SWEEP-26 adjust (ratified 2026-11-18)
// Owner, verbatim: «preservar swing ao guardar; unswing só em opção explícita de export.» The shipped 26 fix
// made load apply the slot's swing fresh; but save stripped the rackSwingBase records (and had no swing
// metadata), so a swung slot saved as a template lost its swing state: loading into a different swing kept
// the old rendering and the next setRackSwing drifted the notes off the grid; with style jitter a small
// swing was even doubled. Save now carries the swing (metadata + rendered ticks + base records); load
// reconciles once; unswing is an explicit export option. Legacy templates (no metadata) load as before.

namespace {
// The swing delay of an odd 1/16 (the same math as pattern_rack.cpp's swingTicks).
int64_t delayOf(double swing) { return (int64_t)std::llround(std::clamp(swing, 0.0, 0.75) * (kPpq / 4) / 2.0); }
// A song with one kick track: four 1/16 notes a bar, slot s1 swung by `swing` (records written). Returns the track id.
std::string swungSlotSong(Document& d, double swing) {
  d = newEmptyDocument();
  d.bpm = 150;
  d.lengthTicks = 4 * kBar;
  const std::string tid = addTrack(d, "kick", "Kick");
  Track& t = trackById(d, tid);
  Pattern& p = t.patterns[0];
  p.lengthTicks = kBar;
  p.notes.clear();
  for (int i = 0; i < 4; ++i) {
    Note n;
    n.id = "n" + std::to_string(i + 1);
    n.tick = i * (kPpq / 4);
    n.length = kPpq / 8;
    n.pitch = 36;
    n.velocity = 0.8;
    p.notes.push_back(n);
  }
  t.clips = {{"c1", p.id, 0, 4 * kBar, 0, json::object()}};
  ensureRackSlots(d, 0);
  setRackBars(d, "s1", 4);  // a 4-bar slot: the pattern repeats its four 1/16s over the four bars
  if (swing > 0) setRackSwing(d, "s1", swing);
  return tid;
}
std::vector<int64_t> ticksOf(const Pattern& p) {
  std::vector<int64_t> v;
  for (const auto& n : p.notes) v.push_back(n.tick);
  std::sort(v.begin(), v.end());
  return v;
}
// The slot's pattern ticks on a track (empty when missing: the comparison fails loudly, never a crash).
std::vector<int64_t> slotTicks(const Document& doc, const std::string& slotId, const std::string& tid) {
  const auto slots = rackSlots(doc);  // kept alive: findRackSlot points into this vector
  const PatternSlot* s = findRackSlot(slots, slotId);
  const Pattern* p = s ? rackPattern(doc, *s, tid) : nullptr;
  return p ? ticksOf(*p) : std::vector<int64_t>{};
}
// The four 1/16 bases of each bar, the odd ones delayed by the swing: what the slot's pattern must play.
std::vector<int64_t> gridTicks(double swing, int bars) {
  std::vector<int64_t> v;
  for (int b = 0; b < bars; ++b)
    for (int i = 0; i < 4; ++i)
      v.push_back(b * kBar + i * (kPpq / 4) + (i % 2 == 1 ? delayOf(swing) : 0));
  std::sort(v.begin(), v.end());
  return v;
}
}  // namespace

TEST("swing save BUG-SWEEP-26: saving a swung slot preserves its swing state, never strips it") {
  Document d;
  swungSlotSong(d, 0.5);
  const PatternTemplate t = rackTemplateFromSlot(d, "s1", "Mine");
  const json j = rackTemplateToJson(t);
  CHECK_NEAR(j.value("swing", 0.0), 0.5, 1e-9);  // before: the swing was dropped on save
  // The rendered ticks stay AND the base records travel with them (load reconciles without double swing).
  bool found = false;
  for (const auto& n : t.rows[0].notes)
    if (n.tick == 240 + delayOf(0.5)) {  // the odd 1/16 at base 240, rendered later
      found = true;
      CHECK(n.extra.contains("rackSwingBase"));  // before: extra wiped, the base lost
      if (n.extra.contains("rackSwingBase") && n.extra["rackSwingBase"].is_object())
        CHECK_EQ(n.extra["rackSwingBase"].value("tick", (int64_t)-1), (int64_t)240);
    }
  CHECK(found);
  // The on-disk form carries both: save, load back, same swing and records.
  const fs::path dir = freshDir("swing26-save");
  saveRackTemplate(dir, t);
  std::vector<std::string> problems;
  const auto loaded = loadRackTemplates(dir, &problems);
  CHECK(problems.empty());
  CHECK_EQ(loaded.size(), (size_t)1);
  if (!loaded.empty()) {
    CHECK_NEAR(rackTemplateToJson(loaded[0]).value("swing", 0.0), 0.5, 1e-9);
    bool rec = false;
    for (const auto& n : loaded[0].rows[0].notes)
      if (n.tick == 240 + delayOf(0.5) && n.extra.contains("rackSwingBase")) rec = true;
    CHECK(rec);
  }
}

TEST("swing save BUG-SWEEP-26: load applies the saved swing once — same-swing round trip exact, other swing reconciles without drift") {
  Document d;
  const std::string tid = swungSlotSong(d, 0.5);
  const PatternTemplate t = rackTemplateFromSlot(d, "s1", "Mine");
  // Round trip into the same slot: the notes play exactly as saved (no double swing)...
  Document a = d;
  CHECK(applyRackTemplate(a, nullptr, t, "s1", 1) > 0);
  CHECK(slotTicks(a, "s1", tid) == gridTicks(0.5, 4));
  // ...and the swing state stays consistent: turning the slot's swing off brings the true grid back.
  setRackSwing(a, "s1", 0.0);
  CHECK(slotTicks(a, "s1", tid) == gridTicks(0.0, 4));
  // Load into a slot with a DIFFERENT swing: the notes reconcile from the saved base to the slot's swing.
  Document b = d;
  const std::string s2 = addRackSlot(b, "Other", 4);
  setRackSwing(b, s2, 0.75);
  CHECK(applyRackTemplate(b, nullptr, t, s2, 1) > 0);
  CHECK(slotTicks(b, s2, tid) == gridTicks(0.75, 4));  // before: kept the template's 0.5 rendering
  setRackSwing(b, s2, 0.0);
  CHECK(slotTicks(b, s2, tid) == gridTicks(0.0, 4));  // before: drifted off the grid (base 210, not 240)
}

TEST("swing save BUG-SWEEP-26: a small swing with style jitter is never doubled on load") {
  Document d;
  const std::string tid = swungSlotSong(d, 0.1);  // 12 ticks of delay
  PatternTemplate t = rackTemplateFromSlot(d, "s1", "Mine");
  t.style = "human";
  PCoreCatalog c;
  PCoreStyle st;
  st.id = "human"; st.name = "Human"; st.humanize = 0.5;  // jitter 12 >= the 12-tick delay: the old load re-swung
  c.styles.push_back(st);
  Document a = d;
  CHECK(applyRackTemplate(a, &c, t, "s1", 1) > 0);
  CHECK(slotTicks(a, "s1", tid) == gridTicks(0.1, 4));  // before: 252 -> 264, doubled
  setRackSwing(a, "s1", 0.0);
  CHECK(slotTicks(a, "s1", tid) == gridTicks(0.0, 4));
}

TEST("swing save BUG-SWEEP-26: legacy templates (no swing metadata) load exactly as before") {
  // (a) A straight legacy template into a swung slot: the fresh-apply swing goes on once (the shipped 26 fix).
  Document d;
  swungSlotSong(d, 0.0);
  const json j = rackTemplateToJson(rackTemplateFromSlot(d, "s1", "Legacy"));
  CHECK(!j.contains("swing"));  // the old form: no swing key
  Document a;
  const std::string atid = swungSlotSong(a, 0.5);
  CHECK(applyRackTemplate(a, nullptr, rackTemplateFromJson(j), "s1", 1) > 0);
  CHECK(slotTicks(a, "s1", atid) == gridTicks(0.5, 4));
  // (b) An old-save swung template (rendered ticks, records stripped by the old save): the off-grid guard
  // keeps the rendered ticks, as before — no metadata to reconcile, no new behavior forced on it.
  Document old;
  swungSlotSong(old, 0.5);
  PatternTemplate ot = rackTemplateFromSlot(old, "s1", "OldSwung");
  for (auto& row : ot.rows)
    for (auto& n : row.notes) n.extra = json::object();  // what the pre-fix save produced
  json oj = rackTemplateToJson(ot);
  oj.erase("swing");
  Document b;
  const std::string btid = swungSlotSong(b, 0.5);
  CHECK(applyRackTemplate(b, nullptr, rackTemplateFromJson(oj), "s1", 1) > 0);
  CHECK(slotTicks(b, "s1", btid) == gridTicks(0.5, 4));
}

TEST("swing save BUG-SWEEP-26: the explicit unswing export option strips the swing; the default keeps it") {
  Document d;
  swungSlotSong(d, 0.5);
  const PatternTemplate kept = rackTemplateFromSlot(d, "s1", "Kept");
  CHECK_NEAR(kept.swing, 0.5, 1e-9);  // the default save preserves the swing
  const PatternTemplate straight = rackTemplateFromSlot(d, "s1", "Straight", true);
  CHECK_NEAR(straight.swing, 0.0, 0);
  CHECK(!rackTemplateToJson(straight).contains("swing"));  // the old form: nothing to reconcile
  std::vector<int64_t> ticks;
  for (const auto& n : straight.rows[0].notes) {
    ticks.push_back(n.tick);
    CHECK(!n.extra.contains("rackSwingBase"));  // rendered straight at the bases, records gone
  }
  std::sort(ticks.begin(), ticks.end());
  CHECK(ticks == gridTicks(0.0, 4));
  // The straight export loads into a swung slot and takes its swing once (the fresh-apply path).
  Document a;
  const std::string atid = swungSlotSong(a, 0.5);
  CHECK(applyRackTemplate(a, nullptr, straight, "s1", 1) > 0);
  CHECK(slotTicks(a, "s1", atid) == gridTicks(0.5, 4));
  setRackSwing(a, "s1", 0.0);
  CHECK(slotTicks(a, "s1", atid) == gridTicks(0.0, 4));
}
// BUG-SWEEP-09 (2026-11-18): the Add Track and Replace instrument menus offered Core's own instruments and a
// sampler track. Core makes no sound of its own (owner 2026-11-18): addTrack always creates kind "none" (a silent
// track waiting for a plugin), so those entries promised a sound the engine no longer makes (PROD-06). The
// menus offer only external plugins, an audio track and import. The offer model is MenuIds.h (extracted from the
// two menu lambdas); the built-in id bands are gone with the built-in instruments.
TEST("BUG-SWEEP-09: the track menus offer no built-in instrument (each would create a silent, unassigned track)") {
  const std::vector<int> add = menu::addTrackFixedItems();
  const std::vector<int> repl = menu::replaceFixedItems();
  auto has = [](const std::vector<int>& v, int id) { return std::find(v.begin(), v.end(), id) != v.end(); };
  // The plugin path stays (the uncapped band), and the audio track and import are still offered; nothing built-in.
  CHECK(has(add, menu::kAudioTrack));
  CHECK(has(add, menu::kImportAudio));
  CHECK_EQ(add.size(), (size_t)2);   // audio + import only; no built-in instrument, no sampler track
  CHECK_EQ(repl.size(), (size_t)0);  // Replace instrument offers plugins only, no fixed built-in entry
}

CORE_TEST_MAIN
