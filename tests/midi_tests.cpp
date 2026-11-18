// Testes do MIDI (midi.hpp): exportar a musica como toca (rolls expandidos, loops, fim da musica, tempo) e importar
// .mid para pistas e padroes. Os ficheiros sao gerados nos testes.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "support/check.hpp"
#include "edit.hpp"
#include "follow.hpp"
#include "groove.hpp"
#include "midi.hpp"
#include "pattern_rack.hpp"
#include "pattern_tools.hpp"
#include "storage.hpp"

using namespace core::doc;
namespace fs = std::filesystem;

namespace {
constexpr int64_t kBar = 4 * kPpq;
// A song with one track: a 1-bar pattern looping over 2 bars, one plain note and one note with a 4-hit roll.
Document rollSong() {
  Document d = newEmptyDocument();
  d.bpm = 150;
  d.lengthTicks = 2 * kBar;
  const std::string id = addTrack(d, "", "Snare");
  Track& t = trackById(d, id);
  Pattern& p = t.patterns[0];
  p.lengthTicks = kBar;
  Note a; a.id = "n1"; a.tick = 0; a.length = 480; a.pitch = 38; a.velocity = 1.0;
  Note r; r.id = "n2"; r.tick = 2 * kPpq; r.length = kPpq; r.pitch = 40; r.velocity = 0.5;
  r.hasRoll = true; r.roll.count = 4; r.roll.velocityFrom = 0.25; r.roll.velocityTo = 1.0;
  p.notes = {a, r};
  t.clips = {{"c1", p.id, 0, 2 * kBar, 0, json::object()}};
  return d;
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
}  // namespace

TEST("MIDI exportar: os rolls saem expandidos como o motor os toca, em cada repeticao do loop") {
  const MidiFile f = songToMidi(rollSong());
  CHECK_EQ(f.ppq, 960);
  CHECK_EQ(f.tracks.size(), (size_t)1);
  CHECK_EQ(f.tracks[0].name, std::string("Snare"));
  const auto& n = f.tracks[0].notes;
  CHECK_EQ(n.size(), (size_t)10);  // (1 + 4) por compasso, 2 compassos
  // Compasso 1: a nota simples, depois 4 batidas de 1/16 a 85%.
  CHECK_EQ(n[0].tick, (int64_t)0);
  CHECK_EQ(n[0].length, (int64_t)480);
  CHECK_EQ(n[0].velocity, 127);
  for (int i = 0; i < 4; ++i) {
    CHECK_EQ(n[(size_t)1 + i].tick, (int64_t)(2 * kPpq + i * 240));
    CHECK_EQ(n[(size_t)1 + i].length, (int64_t)204);  // 240 * 0.85
    CHECK_EQ(n[(size_t)1 + i].pitch, 40);
  }
  CHECK_EQ(n[1].velocity, 32);    // 0.25 * 127
  CHECK_EQ(n[4].velocity, 127);   // 1.0
  CHECK_EQ(n[5].tick, kBar);      // a repeticao
  CHECK_EQ(n[9].tick, kBar + 2 * kPpq + 3 * 240);
}

TEST("MIDI exportar: o fim da musica corta, pistas mudas ficam de fora, o tempo e as rampas vao no ficheiro") {
  Document d = rollSong();
  d.lengthTicks = kBar + kPpq;  // corta a meio do 2o compasso
  const MidiFile f = songToMidi(d);
  CHECK_EQ(f.tracks[0].notes.size(), (size_t)6);  // compasso 1 inteiro + a nota simples do 2o
  CHECK_EQ(f.tracks[0].notes.back().length, (int64_t)480);
  d.lengthTicks = kBar + 240;
  CHECK_EQ(songToMidi(d).tracks[0].notes.back().length, (int64_t)240);  // cortada pelo fim
  d.tracks[0].mix.mute = true;
  CHECK(songToMidi(d).tracks.empty());
  MidiExportOptions all;
  all.mutedTracks = true;
  CHECK_EQ(songToMidi(d, all).tracks.size(), (size_t)1);
  Document t = rollSong();
  t.tempoMap = {{kBar, 180, json::object(), true}};
  const MidiFile g = songToMidi(t);
  CHECK(g.tempos.size() > 4);
  CHECK_NEAR(g.tempos.back().bpm, 180, 1e-9);
  CHECK(g.tempos[1].bpm > 150 && g.tempos[1].bpm < 180);
}

TEST("MIDI: escrever e ler da as mesmas notas e o mesmo tempo") {
  const MidiFile f = songToMidi(rollSong());
  const MidiFile back = parseMidi(writeMidi(f));
  CHECK_EQ(back.ppq, 960);
  CHECK_EQ(back.tracks.size(), (size_t)1);
  CHECK_EQ(back.tracks[0].name, std::string("Snare"));
  CHECK_EQ(back.tracks[0].notes.size(), f.tracks[0].notes.size());
  for (size_t i = 0; i < f.tracks[0].notes.size(); ++i) {
    CHECK_EQ(back.tracks[0].notes[i].tick, f.tracks[0].notes[i].tick);
    CHECK_EQ(back.tracks[0].notes[i].length, f.tracks[0].notes[i].length);
    CHECK_EQ(back.tracks[0].notes[i].pitch, f.tracks[0].notes[i].pitch);
    CHECK_EQ(back.tracks[0].notes[i].velocity, f.tracks[0].notes[i].velocity);
  }
  CHECK(!back.tempos.empty());
  CHECK_NEAR(back.tempos[0].bpm, 150, 0.01);
  // Ficheiro em disco.
  const fs::path file = fs::temp_directory_path() / "core-midi-test.mid";
  saveMidiFile(exportMidi(rollSong()), file);
  CHECK_EQ(parseMidi(readMidiFile(file)).tracks[0].notes.size(), (size_t)10);
}

TEST("MIDI ler: running status, note-on com velocidade 0, formato 0 com dois canais, 96 ticks por seminima") {
  // Formato 0, 96 ppq: canal 1 (36) e canal 10 (42), running status e note-on 0 como note-off.
  const std::vector<uint8_t> bytes = {
      'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
      'M', 'T', 'r', 'k', 0, 0, 0, 30,
      0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,  // 120 BPM
      0x00, 0x90, 36, 100,
      0x00, 42, 0,                                // running status: nota 42 com vel 0 (nada a fechar)
      0x00, 0x99, 42, 90,
      0x30, 0x90, 36, 0,                          // 48 ticks depois: fecha o 36
      0x00, 0x89, 42, 0,
      0x00, 0xFF, 0x2F, 0x00};
  const MidiFile f = parseMidi(bytes);
  CHECK_EQ(f.ppq, 96);
  CHECK_EQ(f.tracks.size(), (size_t)2);  // separado por canal
  CHECK_NEAR(f.tempos.at(0).bpm, 120, 1e-6);
  CHECK_EQ(f.tracks[0].notes.at(0).length, (int64_t)48);
  CHECK_EQ(f.tracks[1].notes.at(0).pitch, 42);
  CHECK_THROWS(parseMidi({'R', 'I', 'F', 'F'}));
  CHECK_THROWS(parseMidi(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 30)));  // cortado
}

TEST("MIDI importar: cada pista vira uma pista do Core com padroes; compassos iguais sao o mesmo padrao") {
  MidiFile f;
  f.ppq = 480;
  MidiTrack bass;
  bass.name = "Bass";
  for (int bar = 0; bar < 8; ++bar)  // 8 compassos, os 4 primeiros iguais aos 4 seguintes
    for (int s = 0; s < 4; ++s) bass.notes.push_back({(int64_t)(bar * 4 + s) * 480, 240, (bar % 4) == 3 ? 38 : 36, 100, 0});
  f.tracks = {bass, MidiTrack{"vazia", {}}};
  f.tempos = {{0, 145}};
  Document d = newEmptyDocument();
  d.lengthTicks = 4 * kBar;
  MidiImportOptions o;
  o.useTempo = true;
  const MidiImportResult r = importMidi(d, f, o);
  CHECK_EQ(r.trackIds.size(), (size_t)1);
  CHECK_EQ(r.patterns, 1);
  CHECK_EQ(r.clips, 2);
  CHECK_EQ(r.notes, 32);
  CHECK(r.songLengthened);
  CHECK_EQ(d.lengthTicks, 8 * kBar);
  CHECK_NEAR(d.bpm, 145, 1e-9);
  const Track& t = trackById(d, r.trackIds[0]);
  CHECK_EQ(t.name, std::string("Bass"));
  CHECK_EQ(t.instrument.kind, std::string("none"));
  CHECK_EQ(t.patterns[0].notes[0].length, (int64_t)480);  // 240 a 480 ppq -> 480 a 960
  // E toca o mesmo: exportar devolve as mesmas notas, nos mesmos tempos.
  const MidiFile back = songToMidi(d);
  CHECK_EQ(back.tracks[0].notes.size(), (size_t)32);
  CHECK_EQ(back.tracks[0].notes[13].tick, (int64_t)13 * 960);
  // A musica continua valida (guarda e reabre).
  CHECK_EQ(toJson(parseDocument(toJson(d))).dump(), toJson(d).dump());
}

TEST("MIDI importar: nomes estragados ficam validos; sem notas e recusado sem mexer em nada") {
  MidiFile f;
  MidiTrack t;
  t.name = std::string("Ba\xE7o \x01", 6);  // Latin-1 c cedilhado e um caracter de controlo
  t.notes = {{0, 960, 60, 90, 0}};
  f.tracks = {t};
  Document d = newEmptyDocument();
  const MidiImportResult r = importMidi(d, f);
  const std::string name = trackById(d, r.trackIds[0]).name;
  CHECK_EQ(name, std::string("Ba\xC3\xA7o"));
  CHECK_EQ(toJson(parseDocument(toJson(d))).dump(), toJson(d).dump());
  Document e = newEmptyDocument();
  const std::string before = toJson(e).dump();
  CHECK_THROWS(importMidi(e, MidiFile{}));
  MidiImportOptions bad;
  bad.barsPerPattern = 0;
  CHECK_THROWS(importMidi(e, f, bad));
  CHECK_EQ(toJson(e).dump(), before);
}

TEST("MIDI importar para um padrao: notas no padrao a partir de um ponto, as de fora ficam de fora") {
  MidiFile f;
  f.ppq = 960;
  f.tracks = {MidiTrack{"x", {{0, 240, 60, 100, 0}, {960, 240, 62, 100, 0}, {8 * 960, 240, 64, 100, 0}}}};
  Pattern p;
  p.id = "p1";
  p.lengthTicks = kBar;
  CHECK_EQ(importMidiIntoPattern(p, f, 960), 2);  // a terceira passa o fim
  CHECK_EQ(p.notes.size(), (size_t)2);
  CHECK_EQ(p.notes[0].tick, (int64_t)960);
  CHECK(p.notes[0].id != p.notes[1].id);
  CHECK_THROWS(importMidiIntoPattern(p, f, 0, 3));
}

// Auditoria 2026-11-17, item 2: com o cabecalho a prometer mais pistas do que as que ha nos bytes, o leitor
// parava em silencio e a app dizia "Importadas N pistas" com uma lista mais curta. midi.hpp promete DocumentError.
TEST("MIDI ler: o cabecalho a prometer mais pistas do que as que o ficheiro tem da erro") {
  // Uma MTrk completa (19 bytes): tempo, uma nota, a sua pausa e o fim da pista.
  const std::vector<uint8_t> chunk = {'M', 'T', 'r', 'k', 0, 0, 0, 19,
                                      0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,   // 120 BPM
                                      0x00, 0x90, 60, 100,
                                      0x60, 0x80, 60, 0,
                                      0x00, 0xFF, 0x2F, 0x00};
  // Formato 1, 960 por seminima; a cabecalho di 2 pistas, so uma esta aqui.
  std::vector<uint8_t> cut = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x03, 0xC0};
  cut.insert(cut.end(), chunk.begin(), chunk.end());
  CHECK_THROWS(parseMidi(cut));
  std::string message;
  try {
    (void)parseMidi(cut);
  } catch (const DocumentError& e) {
    message = e.what();
  }
  CHECK(message.find("1 of 2") != std::string::npos);  // o que se leu contra o que se prometeu
  // O mesmo ficheiro com o numero de pistas certo le-se.
  std::vector<uint8_t> whole = cut;
  whole[11] = 1;
  const MidiFile f = parseMidi(whole);
  CHECK_EQ(f.ppq, 960);
  CHECK_EQ(f.tracks.size(), (size_t)1);
  CHECK_EQ(f.tracks[0].notes.size(), (size_t)1);
  // Pistas a mais do que as prometidas, e bytes depois delas, continua a ler-se.
  std::vector<uint8_t> more = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, 0x03, 0xC0};
  more.insert(more.end(), chunk.begin(), chunk.end());
  more.insert(more.end(), chunk.begin(), chunk.end());
  CHECK_EQ(parseMidi(more).tracks.size(), (size_t)1);
  // Um chunk que nao e MTrk no meio nao conta como pista: as duas leem-se.
  std::vector<uint8_t> listed = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x03, 0xC0};
  listed.insert(listed.end(), chunk.begin(), chunk.end());
  const std::vector<uint8_t> listChunk = {'L', 'I', 'S', 'T', 0, 0, 0, 4, 'i', 'n', 'f', 'o'};
  listed.insert(listed.end(), listChunk.begin(), listChunk.end());
  listed.insert(listed.end(), chunk.begin(), chunk.end());
  const MidiFile both = parseMidi(listed);
  CHECK_EQ(both.tracks.size(), (size_t)2);
}

// BUG-SWEEP-18 (2026-11-18): cleanName checked the lead/continuation structure only. Overlong sequences (E0 80 80),
// surrogate halves (ED A0 80) and code points above U+10FFFF (F4 90 80 80) passed as "valid UTF-8": the import guard
// (parseDocument(toJson(next))) never dumps, so the import succeeded, but saveFile's .dump(2) and the autosave's
// .dump() throw on invalid UTF-8 (nlohmann, strict): the song could not be saved any more.
TEST("MIDI import BUG-SWEEP-18: overlong, surrogate and above-U+10FFFF names keep the song savable") {
  const std::vector<std::string> bad = {
      std::string("\xE0\x80\x80", 3),      // overlong 3-byte sequence
      std::string("\xED\xA0\x80", 3),      // surrogate half U+D800
      std::string("\xF4\x90\x80\x80", 4),  // above U+10FFFF
      std::string("\xF0\x80\x80\x80", 4),  // overlong 4-byte sequence
      std::string("\xF5\x80\x80\x80", 4),  // lead byte beyond F4
      std::string("Kick\xED\xA0\x80" "Drums")};  // invalid bytes inside an otherwise plain name
  for (const std::string& raw : bad) {
    MidiFile f;
    f.ppq = 960;
    MidiTrack t;
    t.name = raw;
    t.notes = {{0, 960, 60, 90, 0}};
    f.tracks = {t};
    Document d = newEmptyDocument();
    const MidiImportResult r = importMidi(d, f);  // the import itself succeeds
    CHECK_EQ(r.trackIds.size(), (size_t)1);
    bool threw = false;
    std::string text;
    try { text = toJson(d).dump(); } catch (const std::exception&) { threw = true; }
    CHECK(!threw);  // before: nlohmann type_error.316 (invalid UTF-8) -> saveFile and the autosave throw
    if (threw) continue;
    CHECK_EQ(toJson(parseDocument(toJson(d))).dump(), text);  // round trip stable
    // The real failure path: save to disk (.dump(2)) and reopen.
    const fs::path file = fs::temp_directory_path() / "core-midi-sweep18.core";
    saveFile(d, file);
    Document back = openFile(file).document;
    CHECK_EQ(trackById(back, r.trackIds[0]).name, trackById(d, r.trackIds[0]).name);
    fs::remove(file);
    fs::remove(backupPath(file));
  }
  // The Latin-1 fallback stays: each invalid byte is read as Latin-1, valid text around it is kept.
  MidiFile f;
  f.ppq = 960;
  MidiTrack t;
  t.name = std::string("Kick\xED\xA0\x80", 7);
  t.notes = {{0, 960, 60, 90, 0}};
  f.tracks = {t};
  Document d = newEmptyDocument();
  const MidiImportResult r = importMidi(d, f);
  CHECK_EQ(trackById(d, r.trackIds[0]).name, std::string("Kick\xC3\xAD\xC2\xA0\xC2\x80"));
}

TEST("rename BUG-SWEEP-18: a track name that is not valid UTF-8 is refused, the song stays savable") {
  Document d = newEmptyDocument();
  const std::string id = addTrack(d, "", "Kick");
  for (const std::string bad : {std::string("\xE0\x80\x80", 3), std::string("\xED\xA0\x80", 3),
                                std::string("\xF4\x90\x80\x80", 4), std::string("Bass\xC0\xAF")}) {
    CHECK_THROWS(renameTrack(d, id, bad));
    CHECK_EQ(trackById(d, id).name, std::string("Kick"));  // nothing changed
  }
  renameTrack(d, id, "Ba\xC3\xA7o");  // valid UTF-8 still goes through
  CHECK_EQ(trackById(d, id).name, std::string("Ba\xC3\xA7o"));
  bool threw = false;
  try { (void)toJson(d).dump(); } catch (const std::exception&) { threw = true; }
  CHECK(!threw);
}

// BUG-SWEEP-30 (external audit 2026-11-18): Louder/Softer (scaleVelocities) moved a roll note's base
// velocity but not the envelope the exporter and the engine actually play (roll.velocityFrom/To when
// count > 1), so the edit was silent on rolls: the exported MIDI stayed byte-identical. The envelope
// must scale by the same factor the base moved by (proportional, shape-preserving), clamped to [0,1].
TEST("MIDI SWEEP-30: Louder and Softer scale a roll's played hit velocities with its base") {
  // Louder (1.15) on the 4-hit roll: base 0.5 -> 0.57 (2dp), envelope 0.25..1 -> 0.285..1 (to clamped).
  Document d = rollSong();
  Pattern& p = d.tracks[0].patterns[0];
  const std::vector<uint8_t> before = exportMidi(d);
  CHECK_EQ(scaleVelocities(p, {}, 1.15), 1);  // n1 is already at 1.0: only the roll note moves
  CHECK_NEAR(p.notes[1].velocity, 0.57, 1e-9);
  CHECK_NEAR(p.notes[1].roll.velocityFrom, 0.285, 1e-9);  // 0.25 * (0.57 / 0.5)
  CHECK_NEAR(p.notes[1].roll.velocityTo, 1.0, 1e-9);      // 1.0 * 1.14 clamped at 1
  const std::vector<uint8_t> after = exportMidi(d);
  CHECK(before != after);  // the bug: the exported bytes were identical after Louder on a roll
  const MidiFile f = songToMidi(d);  // named copy: a reference into the temporary would dangle
  const auto& n = f.tracks[0].notes;
  CHECK_EQ(n.size(), (size_t)10);
  CHECK_EQ(n[0].velocity, 127);  // the ordinary note at 1.0 stays
  const int louder[4] = {36, 66, 97, 127};  // 0.285..1 interpolated over 4 hits, * 127 rounded
  for (int i = 0; i < 4; ++i) {
    CHECK_EQ(n[(size_t)1 + i].velocity, louder[i]);
    CHECK_EQ(n[(size_t)6 + i].velocity, louder[i]);  // the loop repeats the same hits (n[5] is bar 2's ordinary note)
  }
  // Softer (0.85) from the original: base 0.5 -> 0.43, envelope 0.25..1 -> 0.215..0.86 (no clamp).
  Document s = rollSong();
  Pattern& q = s.tracks[0].patterns[0];
  CHECK_EQ(scaleVelocities(q, {}, 0.85), 2);
  CHECK_NEAR(q.notes[1].roll.velocityFrom, 0.215, 1e-9);
  CHECK_NEAR(q.notes[1].roll.velocityTo, 0.86, 1e-9);
  const MidiFile g = songToMidi(s);
  const auto& m = g.tracks[0].notes;
  const int softer[4] = {27, 55, 82, 109};
  for (int i = 0; i < 4; ++i) CHECK_EQ(m[(size_t)1 + i].velocity, softer[i]);
  CHECK_EQ(m[0].velocity, 108);  // the ordinary note scales too: 0.85 * 127
}

TEST("MIDI SWEEP-30: the ordinary-note control and the single-hit velocity contract stay intact") {
  // Control: the same note with the roll cleared (hasRoll AND the payload, the BUG-SWEEP-19 contract: a
  // cleared roll leaves no stale count behind) is an ordinary note; Louder must keep changing its exported MIDI.
  Document d = rollSong();
  Pattern& p = d.tracks[0].patterns[0];
  p.notes[1].hasRoll = false;
  p.notes[1].roll = Roll{};
  const std::vector<uint8_t> before = exportMidi(d);
  CHECK_EQ(scaleVelocities(p, {}, 1.15), 1);
  CHECK(before != exportMidi(d));
  const MidiFile f = songToMidi(d);
  const auto& n = f.tracks[0].notes;
  CHECK_EQ(n.size(), (size_t)4);  // two ordinary notes per bar, two bars
  CHECK_EQ(n[1].velocity, 72);    // 0.57 * 127
  CHECK_EQ(n[1].length, (int64_t)kPpq);  // the whole note, not 85% roll steps
  CHECK(!p.notes[1].hasRoll);
  CHECK_EQ(p.notes[1].roll.count, 0);
  // Single-hit contract: count 0 or 1 plays n.velocity once; the envelope is dead data and stays untouched.
  for (int count : {0, 1}) {
    Document s = rollSong();
    Pattern& q = s.tracks[0].patterns[0];
    q.notes[1].roll.count = count;  // hasRoll stays true: the consumers read count alone
    CHECK_EQ(scaleVelocities(q, {}, 1.15), 1);
    CHECK_NEAR(q.notes[1].velocity, 0.57, 1e-9);
    CHECK_NEAR(q.notes[1].roll.velocityFrom, 0.25, 1e-9);  // not scaled: count <= 1
    CHECK_NEAR(q.notes[1].roll.velocityTo, 1.0, 1e-9);
    const MidiFile g = songToMidi(s);
    const auto& m = g.tracks[0].notes;
    CHECK_EQ(m.size(), (size_t)4);
    CHECK_EQ(m[1].velocity, 72);
    CHECK_EQ(m[1].length, (int64_t)kPpq);
  }
}

// BUG-SWEEP-16 (work preservation): the export answered success with an empty file when the write failed
// late (after the open, at the flush/close), and replaced the user's previous export without a backup.
TEST("MIDI export: a late write failure throws and the previous file stays byte-identical") {
  const fs::path dir = fs::temp_directory_path() / "core-midi-write-safety";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path file = dir / "song.mid";
  const std::vector<uint8_t> previous = exportMidi(rollSong());
  saveMidiFile(previous, file);
  const std::string before = bytesIn(file);
  CHECK(!before.empty());
  Document other = rollSong();
  other.bpm = 200;  // different bytes: a new export over the same file
  const std::vector<uint8_t> next = exportMidi(other);
  CHECK(next != previous);
  bool threw = false;
  {
    WriteFaultAgain restore;
    setWriteFaultForTests(lateFlushFault);
    try { saveMidiFile(next, file); } catch (const DocumentError&) { threw = true; }
  }
  CHECK(threw);  // before: success, with an empty file in the user's place
  CHECK_EQ(bytesIn(file), before);  // the previous export, byte for byte
  CHECK(fs::file_size(file) > 0);
  CHECK(!fs::exists(fs::path(file.string() + ".tmp")));
  for (const auto& e : fs::directory_iterator(dir)) {  // no empty or temporary file left behind
    CHECK(e.is_regular_file());
    CHECK(fs::file_size(e.path()) > 0);
  }
  CHECK_EQ(parseMidi(readMidiFile(file)).tracks[0].notes.size(), (size_t)10);  // still a readable MIDI
}

TEST("MIDI export: a successful overwrite reads back the same bytes and keeps the previous export as .bak") {
  const fs::path dir = fs::temp_directory_path() / "core-midi-write-safety-bak";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path file = dir / "song.mid";
  const std::vector<uint8_t> previous = exportMidi(rollSong());
  saveMidiFile(previous, file);
  const std::string before = bytesIn(file);
  Document other = rollSong();
  other.bpm = 200;
  const std::vector<uint8_t> next = exportMidi(other);
  saveMidiFile(next, file);
  CHECK_EQ(bytesIn(file), std::string((const char*)next.data(), next.size()));
  CHECK_EQ(bytesIn(backupPath(file)), before);  // before: the previous export was gone, no backup
  CHECK_NEAR(parseMidi(readMidiFile(file)).tempos[0].bpm, 200, 0.01);
  for (const auto& e : fs::directory_iterator(dir)) CHECK(fs::file_size(e.path()) > 0);
}

// BUG-SWEEP-19 (roll preservation, 2026-11-18): songToMidi read roll.count without hasRoll, and Density's
// octave/ratchet copies cleared hasRoll but kept the copied roll payload. The stale count still exported the
// copies as 4-hit rolls, while the serializer writes roll only when hasRoll: after save/reopen the same song
// exported fewer notes (the export shrank after reopen). The exporter must read the roll only when hasRoll
// says there is one (as the engine projection does), and a cleared roll must leave no payload behind (the
// stutter precedent). Valid rolls keep exporting exactly as before.
TEST("MIDI SWEEP-19: the exporter reads a roll only when hasRoll says there is one") {
  Document d = rollSong();
  d.tracks[0].patterns[0].notes[1].hasRoll = false;  // the payload stays stale (count 4)
  const MidiFile f = songToMidi(d);
  CHECK_EQ(f.tracks[0].notes.size(), (size_t)4);  // before: 10, the stale count still expanded
  CHECK_EQ(f.tracks[0].notes[1].length, (int64_t)kPpq);  // the whole note, as the engine plays it
  d.tracks[0].patterns[0].notes[1].hasRoll = true;  // a valid roll is untouched: the 4 hits return
  CHECK_EQ(songToMidi(d).tracks[0].notes.size(), (size_t)10);
}

TEST("MIDI SWEEP-19: density copies carry no stale roll; the export survives save/reopen unchanged") {
  for (const char* how : {"octave", "ratchet"}) {
    Document d = rollSong();
    Pattern& p = d.tracks[0].patterns[0];
    DensityOptions o;
    o.mode = "count";
    o.amount = 2;
    o.how = how;
    o.seed = 7;
    const DensityResult r = changeDensity(p, wholePattern(p), o);
    CHECK_EQ(r.added, 2);
    for (const auto& n : p.notes)
      if (!n.hasRoll) CHECK_EQ(n.roll.count, 0);  // before: the copy kept the copied count 4
    const size_t before = songToMidi(d).tracks[0].notes.size();
    const Document reopened = parseDocument(toJson(d));
    const size_t after = songToMidi(reopened).tracks[0].notes.size();
    CHECK_EQ(after, before);  // before: 20 exported, 14 after reopen (the stale roll did not survive the save)
  }
}

TEST("MIDI SWEEP-19: a preset cut from a rolled pattern carries no stale roll payload") {
  const Document d = rollSong();
  const PatternPreset p = presetFromPattern(d.tracks[0].patterns[0], 0, kBar, "rolled");
  CHECK_EQ(p.notes.size(), (size_t)2);
  for (const auto& n : p.notes) {
    CHECK(!n.hasRoll);  // presets stay plain (the existing contract)
    CHECK_EQ(n.roll.count, 0);  // before: the copied payload kept count 4
  }
}

// BUG-SWEEP-30 follow-up (owner ratified 2026-11-18): «escala de envelope consistente em ALL base-writers via
// helper partilhado (ratifica a política para o follow-up avançar).» The 30 fix moved Louder/Softer, the velocity
// lane, the wheel, the Inspector, the Patterns panel and Normalize onto doc::setNoteVelocity; the remaining
// base-writers (humanizeNotes, varyNotes accent, the build-up velocity ramp, follow's write-back, toggleRackStep)
// still wrote only Note.velocity, which a multi-hit roll never plays. All of them now go through the shared
// helper: proportional realized factor, clamp [0,1], zero stays zero, count <= 1 untouched.
TEST("MIDI SWEEP-30 follow-up: humanizeNotes scales a roll's played envelope with its base") {
  Document d = rollSong();
  Pattern& p = d.tracks[0].patterns[0];
  const std::vector<uint8_t> before = exportMidi(d);
  const int changed = humanizeNotes(p, {}, 0, 0.2, 7);  // timing 0: only the velocity moves
  CHECK(changed >= 1);
  const double v = p.notes[1].velocity;
  CHECK(v != 0.5);  // the seed moves the roll note's base (otherwise this case proves nothing)
  const double factor = v / 0.5;  // the realized factor, after humanize's own rounding/clamping
  CHECK_NEAR(p.notes[1].roll.velocityFrom, std::clamp(0.25 * factor, 0.0, 1.0), 1e-9);
  CHECK_NEAR(p.notes[1].roll.velocityTo, std::clamp(1.0 * factor, 0.0, 1.0), 1e-9);
  CHECK_EQ(p.notes[1].roll.count, 4);  // the hit count is untouched
  CHECK(before != exportMidi(d));  // the bug: the exported bytes stayed identical on a roll
  const MidiFile f = songToMidi(d);
  const auto& n = f.tracks[0].notes;
  CHECK_EQ(n.size(), (size_t)10);
  for (int i = 0; i < 4; ++i) {
    const double vel = p.notes[1].roll.velocityFrom + (p.notes[1].roll.velocityTo - p.notes[1].roll.velocityFrom) * (double)i / 3.0;
    const int expected = std::clamp((int)std::lround(std::min(1.0, vel) * 127.0), 1, 127);
    CHECK_EQ(n[(size_t)1 + i].velocity, expected);
    CHECK_EQ(n[(size_t)6 + i].velocity, expected);  // the loop repeats the same hits
  }
  // Single-note contract: count <= 1 plays the base once; the dead envelope stays untouched.
  Document s = rollSong();
  Pattern& q = s.tracks[0].patterns[0];
  q.notes[1].roll.count = 1;
  humanizeNotes(q, {}, 0, 0.2, 7);
  CHECK_NEAR(q.notes[1].velocity, v, 1e-9);  // the same seed, the same base move
  CHECK_NEAR(q.notes[1].roll.velocityFrom, 0.25, 1e-9);
  CHECK_NEAR(q.notes[1].roll.velocityTo, 1.0, 1e-9);
}

TEST("MIDI SWEEP-30 follow-up: varyNotes accent scales a roll's played envelope with its base") {
  Document d = rollSong();
  Pattern& p = d.tracks[0].patterns[0];
  const std::vector<uint8_t> before = exportMidi(d);
  // The roll note sits on the beat (tick 960): accent lifts 0.5 -> 0.65, realized factor 1.3.
  CHECK_EQ(varyNotes(p, {}, "accent", 1), 1);  // n1 is already at 1.0 (clamped): only the roll note moves
  CHECK_NEAR(p.notes[1].velocity, 0.65, 1e-9);
  CHECK_NEAR(p.notes[1].roll.velocityFrom, 0.325, 1e-9);  // 0.25 * 1.3
  CHECK_NEAR(p.notes[1].roll.velocityTo, 1.0, 1e-9);      // 1.0 * 1.3 clamped at 1
  CHECK(before != exportMidi(d));
  const MidiFile f = songToMidi(d);
  const auto& n = f.tracks[0].notes;
  CHECK_EQ(n.size(), (size_t)10);
  const int accented[4] = {41, 70, 98, 127};  // 0.325..1 over 4 hits, * 127 rounded
  for (int i = 0; i < 4; ++i) {
    CHECK_EQ(n[(size_t)1 + i].velocity, accented[i]);
    CHECK_EQ(n[(size_t)6 + i].velocity, accented[i]);
  }
  CHECK_EQ(n[0].velocity, 127);  // the ordinary note stays at 1.0
  // Off the beat the accent softens: the same proportional rule (0.5 -> 0.4, factor 0.8, no clamp).
  Document s = rollSong();
  Pattern& q = s.tracks[0].patterns[0];
  q.notes[1].tick = 2 * kPpq + kPpq / 2;
  CHECK_EQ(varyNotes(q, {}, "accent", 1), 1);
  CHECK_NEAR(q.notes[1].velocity, 0.4, 1e-9);
  CHECK_NEAR(q.notes[1].roll.velocityFrom, 0.2, 1e-9);  // 0.25 * 0.8
  CHECK_NEAR(q.notes[1].roll.velocityTo, 0.8, 1e-9);    // 1.0 * 0.8
  // Single-note contract: count <= 1 keeps the dead envelope untouched.
  Document u = rollSong();
  Pattern& r = u.tracks[0].patterns[0];
  r.notes[1].roll.count = 1;
  CHECK_EQ(varyNotes(r, {}, "accent", 1), 1);
  CHECK_NEAR(r.notes[1].velocity, 0.65, 1e-9);
  CHECK_NEAR(r.notes[1].roll.velocityFrom, 0.25, 1e-9);
  CHECK_NEAR(r.notes[1].roll.velocityTo, 1.0, 1e-9);
}

TEST("MIDI SWEEP-30 follow-up: buildup ramp, rack steps and follow write through the shared helper unchanged") {
  // makeBuildup's velocity ramp writes NEW notes: the ramp is the base velocity, no envelope (single-note
  // contract). The shared helper must keep the ramp exactly as before, and the export one hit per note.
  Document d = newEmptyDocument();
  d.lengthTicks = 4 * kBar;
  const std::string id = addTrack(d, "", "Snare");
  Track& t = trackById(d, id);
  Pattern& p = t.patterns[0];
  p.lengthTicks = 4 * kBar;
  p.notes.clear();
  t.clips = {{"c1", p.id, 0, 4 * kBar, 0, json::object()}};
  BuildupOptions o;
  o.start = 0; o.length = 2 * kBar; o.pitch = 38; o.velocityFrom = 0.3; o.velocityTo = 0.9;
  const int written = makeBuildup(p, o);
  CHECK(written > 4);
  CHECK_EQ((int)p.notes.size(), written);
  double last = 0;
  for (const auto& n : p.notes) {
    CHECK(!n.hasRoll);
    CHECK_EQ(n.roll.count, 0);  // a new note: the envelope is dead data, untouched
    CHECK(n.velocity >= 0.3 && n.velocity <= 0.9);
    CHECK(n.velocity >= last);  // the ramp rises
    last = n.velocity;
  }
  CHECK_NEAR(p.notes.front().velocity, 0.3, 1e-9);
  const MidiFile f = songToMidi(d);
  CHECK_EQ(f.tracks[0].notes.size(), p.notes.size());  // one hit per note: no roll expansion
  // toggleRackStep writes a NEW note with the clamped velocity: no envelope, exactly as before.
  Pattern r;
  r.lengthTicks = kBar;
  CHECK(toggleRackStep(r, 0, kPpq / 4, 36, 0.73, 0.0));
  CHECK_EQ(r.notes.size(), (size_t)1);
  CHECK_NEAR(r.notes[0].velocity, 0.73, 1e-9);
  CHECK(!r.notes[0].hasRoll);
  CHECK_EQ(r.notes[0].roll.count, 0);
  // follow: a rolled note in the margin the offset needs is written back with its base unchanged (factor 1):
  // the shared helper keeps its envelope exactly; the zone's new notes are plain.
  Document g = rollSong();
  Track& gt = g.tracks[0];
  Pattern& gp = gt.patterns[0];
  gp.lengthTicks = 2 * kBar;
  Note m; m.id = "n3"; m.tick = 1850; m.length = 60; m.pitch = 36; m.velocity = 0.5;
  m.hasRoll = true; m.roll.count = 4; m.roll.velocityFrom = 0.25; m.roll.velocityTo = 1.0;
  gp.notes.push_back(m);
  gt.clips = {{"c1", gp.id, 0, 2 * kBar, 0, json::object()}};
  g.lengthTicks = 2 * kBar;
  FollowSettings fs;
  fs.offsetTicks = -120; fs.looseTicks = 0; fs.seed = 1;  // the zone [1920, 2160) writes from 1800: 1850 is margin
  const std::vector<FollowZone> zones = {{1920, 2160, {1920}}};
  CHECK(kickFollow(g, gt.id, zones, fs) > 0);
  const Note* kept = nullptr;
  for (const auto& pat : gt.patterns)
    for (const auto& n : pat.notes)
      if (n.pitch == 36 && n.hasRoll) kept = &n;
  CHECK(kept != nullptr);  // the margin's roll survived the write-back
  if (kept) {
    CHECK_NEAR(kept->velocity, 0.5, 1e-9);
    CHECK_EQ(kept->roll.count, 4);
    CHECK_NEAR(kept->roll.velocityFrom, 0.25, 1e-9);
    CHECK_NEAR(kept->roll.velocityTo, 1.0, 1e-9);
  }
}

CORE_TEST_MAIN
