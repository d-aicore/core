// Testes dos bugs medios e pequenos corrigidos a 2026-11-16 (lista do sweep antigo revista contra o main 06e0882).
// Cada teste falhava antes da correcao.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include "support/check.hpp"
#include "assets.hpp"
#include "edit.hpp"
#include "follow.hpp"
#include "pattern_tools.hpp"
#include "pcore.hpp"
#include "samples.hpp"
#include "stutter.hpp"
#include "storage.hpp"
#include "templates.hpp"
#include "translation.hpp"

using namespace core::doc;
namespace fs = std::filesystem;

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

TEST("reset: o menu da chain oferece TODOS os efeitos (os proprios incluidos) e o Remove leva o FX proprio e as curvas dele") {
  Document d = newEmptyDocument();
  const std::string id = addTrack(d, "", "X");
  const int atUtility = addEffect(d, id, "utility");  // Core's own, as a template or an old song carries them
  json verb = {{"id", "fx9"}, {"type", "plugin"},
               {"plugin", json{{"id", "v1"}, {"name", "FakeVerb"}, {"format", "vst3"}, {"file", "C:/plugins/FakeVerb.vst3"}}},
               {"bypass", false}};
  trackById(d, id).effects.push_back(verb);
  addEffect(d, id, "eq");  // another own one, after the plugin
  const std::vector<EffectMenuEntry> entries = effectMenuEntries(trackById(d, id).effects);
  CHECK_EQ((int)entries.size(), 3);  // the old menus offered only the plugin: the own ones had no Remove anywhere
  if (entries.size() == 3) {  // the detail checks only read what the size check above guarantees
    CHECK_EQ(entries[0].index, atUtility);
    CHECK_EQ(entries[0].label, std::string("Utility"));
    CHECK(!entries[0].plugin);
    CHECK(!entries[0].window);
    CHECK_EQ(entries[1].index, 1);
    CHECK_EQ(entries[1].label, std::string("FakeVerb"));
    CHECK(entries[1].plugin && entries[1].window);
    CHECK_EQ(entries[2].index, 2);
    CHECK_EQ(entries[2].label, std::string("Core EQ"));
  }
  // And the Remove the menu offers works on an own effect: the chain and its curves go together.
  setAutomationPoint(d, id, effectParamKey("fx1", "gain"), 0, -3);
  CHECK(!trackById(d, id).automation.empty());
  removeEffect(d, id, entries[0].index);
  CHECK_EQ((int)trackById(d, id).effects.size(), 2);
  CHECK_EQ(trackById(d, id).effects[0].value("id", std::string()), std::string("fx9"));
  const bool curvesGone = std::none_of(trackById(d, id).automation.begin(), trackById(d, id).automation.end(),
                                       [](const Automation& a) { return a.param == "fx:fx1:gain"; });
  CHECK(curvesGone);
}

// Auditoria 2026-11-17, item 4: duplicateSection empurrava as mudancas de tempo de depois da seccao mas nao
// copiava as de dentro, por isso a seccao duplicada perdia o tempo (e o ramp) que o randomizador EIXO J escreve.
TEST("duplicar uma seccao copia tambem as mudancas de tempo que ha dentro dela") {
  Document d = newEmptyDocument();
  const std::string a = addSection(d, "Build", 0, 4 * kBar);
  addSection(d, "Drop", 4 * kBar, 4 * kBar);
  setTempoMap(d, {{1 * kBar, 200, json::object(), false}, {3 * kBar, 160, json::object(), true}, {6 * kBar, 120, json::object(), false}});
  const size_t before = d.tempoMap.size();
  const std::string dup = duplicateSection(d, a);
  CHECK_EQ(d.tempoMap.size(), before + 2);  // os dois pontos de dentro tem copia na seccao nova
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
    CHECK(glide->ramp);  // o ramp tambem
  }
  const TempoPoint* later = at(10 * kBar);  // o que ja ia para depois move-se uma seccao
  CHECK(later != nullptr);
  if (later) CHECK_NEAR(later->bpm, 120, 1e-9);
  // O tempo que toca na seccao duplicada e o da seccao original, nao o ultimo de antes.
  CHECK_NEAR(bpmAt(d, 5 * kBar), 200, 1e-9);   // antes: 160, o ponto de 3 compassos, que ficava a valer
  CHECK_NEAR(bpmAt(d, 7 * kBar), 160, 1e-9);
  CHECK(!dup.empty());
  const Section* moved = nullptr;
  for (const auto& s : d.sections) if (s.id == dup) moved = &s;
  CHECK(moved != nullptr);
  if (moved) CHECK_EQ(moved->startTick, 4 * kBar);
}

CORE_TEST_MAIN
