// Characterization guards for the dead-code removal of lots L1/L2 (2026-11-18, dead-code inventory).
// These pin the LIVE behavior the removals must not change: old songs with Core's own instruments,
// sampler zones, synth data, modulators and ducking still open, save and reopen byte for byte, and
// stay out of the engine projection; the five visible templates still open and render; a legacy
// engine project still loads and renders, bit for bit, at any block size.
// Run: legacy_guard_tests [part-of-name]
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include "support/check.hpp"
#include "support/signals.hpp"
#include "core/assets.hpp"
#include "core/project.hpp"
#include "core/server.hpp"
#include "io/wav.hpp"
#include "document.hpp"
#include "edit.hpp"
#include "pcore.hpp"
#include "sample_analysis.hpp"
#include "samples.hpp"
#include "templates.hpp"

using namespace core::doc;
namespace fs = std::filesystem;

namespace {
fs::path tempDir() {
  fs::path d = fs::temp_directory_path() / "core-legacy-guard-tests";
  fs::create_directories(d);
  return d;
}
std::string canon(const Document& d) { return toJson(d).dump(); }

// A song as the app saves it, with the data of the removed built-ins still in it: a kick with an
// unknown extra key and the old fromVst mark, a version-1 Core EQ, a W-Core with its synth links,
// a sampler with a looped zone, and a ducking modulator keyed by the kick.
json legacySongJson() {
  json j = toJson(newTemplateDocument());
  json& tracks = j["tracks"];
  // The basic template's tracks are "none" today (addTrack); an old song wrote its built-in instruments.
  // kick: extra keys (fromVst must not survive a load; the unknown key must) and a version-1 eq.
  json& kick = tracks[0];
  kick["instrument"]["kind"] = "builtin";
  kick["instrument"]["name"] = "kick";
  kick["instrument"]["version"] = 1;
  kick["instrument"]["fromVst"] = true;
  kick["instrument"]["keptKey"] = 7;
  kick["effects"] = json::array({{{"id", "eq1"}, {"type", "builtin"}, {"name", "eq"}, {"version", 1},
                                  {"params", {{"gain", 1.5}}}, {"bypass", false}}});
  // bass: a W-Core (wave) with its synth modulation kept as data.
  json& bass = tracks[1];
  bass["instrument"]["kind"] = "builtin";
  bass["instrument"]["name"] = "wave";
  bass["instrument"]["version"] = 2;
  bass["instrument"]["synth"] = {{"layers", json::array({{{"lfos", json::array()}, {"links", json::array()}}})}};
  // hats: a sampler with one looped zone, an older version written, and a duck modulator from the kick.
  json& hats = tracks[2];
  hats["instrument"]["kind"] = "builtin";
  hats["instrument"]["name"] = "sampler";
  hats["instrument"]["version"] = 2;
  hats["instrument"]["samples"] = json::array({{{"file", "D:\\Samples\\kick.wav"}, {"sha256", std::string(64, 'a')},
                                                {"name", "Kick"}, {"rootKey", 60}, {"loKey", 0}, {"hiKey", 127},
                                                {"loopStart", 0.1}, {"loopEnd", 0.4}}});
  hats["modulators"] = json::array({{{"id", "duck"}, {"param", "gainDb"}, {"shape", "envelope"}, {"rateBeats", 1},
                                     {"depth", -4.0}, {"phase", 0}, {"attackMs", 2.0}, {"decayMs", 120.0},
                                     {"source", kick["id"].get<std::string>()}}});
  return j;
}
}  // namespace

TEST("legacy guard: an old song opens and keeps every stored sound datum") {
  const json j = legacySongJson();
  const Document d = parseDocument(j);
  CHECK_EQ(d.tracks.size(), (size_t)3);
  const Track& kick = d.tracks[0];
  CHECK_EQ(kick.instrument.kind, std::string("builtin"));
  CHECK_EQ(kick.instrument.name, std::string("kick"));
  // dropVst cleans the old mark; any other unknown key stays.
  CHECK(!kick.instrument.extra.contains("fromVst"));
  CHECK_EQ(kick.instrument.extra.value("keptKey", 0), 7);
  // The version written is the version kept: loading never upgrades it (upgradeOldVersions had no caller).
  CHECK_EQ(kick.effects[0].value("version", 0), 1);
  CHECK_EQ(kick.effects[0].value("name", std::string()), std::string("eq"));
  const Track& bass = d.tracks[1];
  CHECK_EQ(bass.instrument.name, std::string("wave"));
  CHECK(bass.instrument.synth.is_object() && bass.instrument.synth.contains("layers"));
  const Track& hats = d.tracks[2];
  CHECK_EQ(hats.instrument.name, std::string("sampler"));
  CHECK_EQ(hats.instrument.version, 2);  // not bumped to kSamplerVersion
  CHECK_EQ(hats.instrument.samples.size(), (size_t)1);
  if (!hats.instrument.samples.empty()) {
    CHECK_NEAR(hats.instrument.samples[0].loopStart, 0.1, 1e-12);
    CHECK_NEAR(hats.instrument.samples[0].loopEnd, 0.4, 1e-12);
  }
  // The ducking modulator stays in the song (the format still reads and writes it).
  CHECK_EQ(hats.modulators.size(), (size_t)1);
  CHECK_EQ(hats.modulators[0].value("id", std::string()), std::string("duck"));
  CHECK_EQ(hats.modulators[0].value("source", std::string()), kick.id);
  // Save and reopen: byte for byte the same.
  CHECK_EQ(canon(parseDocument(toJson(d))), canon(d));
}

TEST("legacy guard: the projection sends none of the removed built-ins to the engine") {
  const Document d = parseDocument(legacySongJson());
  const json p = toProjection(d);
  for (const auto& t : p["tracks"]) {
    CHECK_EQ(t.value("instrument", std::string()), std::string("none"));
    CHECK(!t.contains("modulators"));
    CHECK(!t.contains("synth"));
    for (const auto& fx : t.value("effects", json::array()))
      CHECK_EQ(fx.value("type", std::string()), std::string("plugin"));
  }
  // The engine accepts the projection of an old song.
  const core::Project proj = core::fromJson(p);
  CHECK_EQ(proj.tracks.size(), (size_t)3);
}

TEST("legacy guard: the five visible templates open, project and render finite") {
  const fs::path user = tempDir() / "user-empty";
  fs::create_directories(user);
  const TemplateLibrary lib = loadTemplates(CORE_TEMPLATES_DIR, user, false);
  for (const auto& p : lib.problems) coretest::fail(p.file.string().c_str(), 0, p.message);
  CHECK_EQ(lib.templates.size(), (size_t)5);  // the owner keeps the five visible ones (2026-11-18)
  for (const auto& e : lib.templates) {
    CHECK(!isBaseTemplate(e.tmpl));
    const Document d = documentFromTemplate(e.tmpl);
    const core::Project p = core::fromJson(toProjection(d));
    core::Project short4 = p;
    short4.lengthTicks = std::min<int64_t>(p.lengthTicks, 4 * 4 * core::kPpq);
    const core::RenderResult r = core::renderProject(short4, 48000, 512, false, 0.1, nullptr, nullptr, nullptr, 0);
    bool finite = true;
    for (float s : r.L) finite = finite && std::isfinite(s);
    for (float s : r.R) finite = finite && std::isfinite(s);
    CHECK(finite);
  }
}

TEST("legacy guard: the engine loads a legacy project (sampler zones, synth, bus with samples)") {
  const json zone = {{"path", "D:\\Samples\\kick.wav"}, {"rootKey", 60}, {"loKey", 0}, {"hiKey", 127},
                     {"loopStart", 0.0}, {"loopEnd", 0.2}};
  const json project = {
      {"format", core::kProjectFormat}, {"version", 2}, {"name", "legacy"}, {"bpm", 145.0},
      {"lengthTicks", 4 * 4 * core::kPpq}, {"masterGainDb", 0.0},
      {"tracks", json::array({
          {{"id", "kick"}, {"instrument", "kick"}, {"patternTicks", 4 * core::kPpq},
           {"notes", json::array({{{"tick", 0}, {"length", 240}, {"pitch", 36}, {"velocity", 0.9}}})},
           {"modulators", json::array({{{"id", "m1"}, {"param", "gainDb"}, {"shape", "sine"},
                                        {"rateBeats", 1}, {"depth", -2.0}, {"phase", 0}}})}},
          {{"id", "sam"}, {"instrument", "sampler"}, {"patternTicks", 4 * core::kPpq},
           {"samples", json::array({zone})},
           {"notes", json::array({{{"tick", 0}, {"length", 240}, {"pitch", 36}, {"velocity", 0.9}}})}},
          {{"id", "w"}, {"instrument", "wave"}, {"patternTicks", 4 * core::kPpq},
           {"synth", {{"layers", json::array()}}},
           {"notes", json::array({{{"tick", 0}, {"length", 240}, {"pitch", 33}, {"velocity", 0.8}}})}},
          {{"id", "bus1"}, {"instrument", "bus"}, {"samples", json::array({zone})}}})}};
  const core::Project p = core::fromJson(project);
  CHECK_EQ(p.tracks.size(), (size_t)4);
  for (const auto& t : p.tracks)
    if (t.id != "bus1") CHECK_EQ(t.instrument, std::string("none"));
  // The sampler's sounds are reported, never dropped in silence; the bus with a samples key still loads
  // (the engine never filled Track::samples from JSON).
  bool samplerNote = false, busNote = false;
  for (const auto& n : p.tracks[1].loadNotes) samplerNote = samplerNote || n.find("sampler") != std::string::npos;
  for (const auto& n : p.tracks[3].loadNotes) busNote = busNote || n.find("sampler") != std::string::npos;
  CHECK(samplerNote);
  CHECK(busNote);
  // The gainDb modulator is engine-live and stays.
  CHECK_EQ(p.tracks[0].modulators.size(), (size_t)1);
  const core::RenderResult r = core::renderProject(p, 48000, 512, false, 0.1, nullptr, nullptr, nullptr, 0);
  bool finite = true;
  for (float s : r.L) finite = finite && std::isfinite(s);
  CHECK(finite);
}

TEST("legacy guard: a legacy song with audio renders bit for bit at any block size") {
  const double sr = 48000;
  const auto s = coretest::sine(220.0, sr, (int64_t)(0.5 * sr), 0.4f);
  const std::string wav = (tempDir() / "legacy-guard-sine.wav").string();
  core::writeWav(wav, s, s, (int)sr, 32);
  const json clip = {{"startTick", 0}, {"path", wav}, {"offsetSeconds", 0}, {"durationSeconds", 0.5}, {"gainDb", 0.0}};
  const json project = {
      {"format", core::kProjectFormat}, {"version", 2}, {"name", "legacy-audio"}, {"bpm", 145.0},
      {"lengthTicks", 4 * 4 * core::kPpq}, {"masterGainDb", -3.0},
      {"tracks", json::array({
          {{"id", "audio"}, {"instrument", "audio"}, {"audioClips", json::array({clip})}, {"output", "bus1"}},
          {{"id", "kick"}, {"instrument", "kick"}, {"patternTicks", 4 * core::kPpq}, {"output", "bus1"},
           {"notes", json::array({{{"tick", 0}, {"length", 240}, {"pitch", 36}, {"velocity", 0.9}}})},
           {"modulators", json::array({{{"id", "m1"}, {"param", "gainDb"}, {"shape", "sine"},
                                        {"rateBeats", 1}, {"depth", -2.0}, {"phase", 0}}})}},
          {{"id", "bus1"}, {"instrument", "bus"}, {"gainDb", -2.0}}})}};
  const core::Project p = core::fromJson(project);
  core::AssetStore assets;  // without a store the audio clips stay silent: the null must hear real audio
  const core::RenderResult a = core::renderProject(p, sr, 512, false, 0.2, nullptr, &assets, nullptr, 0);
  const core::RenderResult b = core::renderProject(p, sr, 128, false, 0.2, nullptr, &assets, nullptr, 0);
  CHECK_EQ(a.L.size(), b.L.size());
  CHECK_EQ(a.R.size(), b.R.size());
  CHECK(a.peak > 0.0f);  // the audio clip really sounds (not a silence-vs-silence comparison)
  bool same = a.L.size() == b.L.size();
  for (size_t i = 0; same && i < a.L.size(); ++i) same = a.L[i] == b.L[i] && a.R[i] == b.R[i];
  CHECK(same);
}

// ---- L2 (2026-11-18): the lot removed the modulator and ducking setters, the A/B TrackSound API,
// setEffectSidechain, setSampleLoop, addEffectWith, addPhraseEndComb, resetInstrumentParams,
// instrumentAtDefaults, browseSampleRoles and sampleFitInRegion. The data those setters wrote is still
// the song format: it loads, saves and reopens unchanged and stays out of the projection. The live
// effect path, the pattern catalogue, the kick/bass/cymbal alignment and the sample role catalogue
// keep their behavior. These tests use only live APIs: they pass before and after the removal.

namespace {
json notesJson(const Pattern& p) {
  json a = json::array();
  for (const auto& n : p.notes)
    a.push_back({{"tick", n.tick}, {"length", n.length}, {"pitch", n.pitch}, {"velocity", n.velocity}});
  return a;
}
}  // namespace

TEST("legacy guard L2: modulator, sidechain and A/B sound data survive a save/reopen") {
  json j = legacySongJson();
  json& kick = j["tracks"][0];
  json& bass = j["tracks"][1];
  const std::string kickId = kick["id"].get<std::string>();
  const std::string bassId = bass["id"].get<std::string>();
  // Built-in instrument parameters (resetInstrumentParams/instrumentAtDefaults are gone; the values stay).
  const InstrumentInfo* info = findInstrumentInfo("kick");
  CHECK(info != nullptr);
  if (info && !info->params.empty())
    kick["instrument"]["params"] = json{{info->params[0].name, info->params[0].def}};
  // A compressor that follows another track (setEffectSidechain is gone; the key stays in the song).
  kick["effects"].push_back({{"id", "comp1"}, {"type", "builtin"}, {"name", "compressor"}, {"version", 1},
                             {"params", {{"threshold", -18.0}}}, {"bypass", false}, {"sidechain", bassId}});
  // Every modulator shape the removed setters wrote, one of them turned off.
  bass["modulators"] = json::array({
      {{"id", "m1"}, {"param", "cutoff"}, {"shape", "sine"}, {"rateBeats", 0.25}, {"depth", 1.5}, {"phase", 0.25}, {"on", false}},
      {{"id", "m2"}, {"param", "gainDb"}, {"shape", "steps"}, {"rateBeats", 0.5}, {"depth", 0.8}, {"phase", 0},
       {"steps", json::array({1.0, -0.5, 0.25, 0.0})}},
      {{"id", "m3"}, {"param", "cutoff"}, {"shape", "envelope"}, {"rateBeats", 1}, {"depth", 2.0}, {"phase", 0},
       {"attackMs", 5.0}, {"decayMs", 200.0}, {"source", kickId}},
      {{"id", "m4"}, {"param", "gainDb"}, {"shape", "rise"}, {"rateBeats", 4}, {"depth", 1.0}, {"phase", 0}, {"source", kickId}}});
  // The A/B sound records (TrackSound is gone): racks, FCore, a mood's knobs, and a key that is not sound.
  // They live as the track's own extra keys, the way the song writes them.
  bass["racks"] = json::array({{{"id", "r1"}}});
  bass["fcore"] = json{{"preset", "x"}};
  bass["bassMoodKnobs"] = json{{"mood", 0.5}};
  bass["uiOnly"] = 42;
  const Document d = parseDocument(j);
  const Track& k = d.tracks[0];
  const Track& b = d.tracks[1];
  CHECK_EQ(k.effects.size(), (size_t)2);
  CHECK_EQ(k.effects[1].value("sidechain", std::string()), bassId);
  if (info && !info->params.empty()) {
    const auto it = k.instrument.params.find(info->params[0].name);
    CHECK(it != k.instrument.params.end());
    if (it != k.instrument.params.end()) CHECK_NEAR(it->second, info->params[0].def, 1e-12);
  }
  CHECK_EQ(b.modulators.size(), (size_t)4);
  CHECK_EQ(b.modulators[0].value("on", true), false);
  CHECK_EQ(b.modulators[1]["steps"].size(), (size_t)4);
  CHECK_EQ(b.modulators[2].value("source", std::string()), kickId);
  CHECK_EQ(b.modulators[3].value("shape", std::string()), std::string("rise"));
  CHECK(b.extra.contains("racks") && b.extra.contains("fcore") && b.extra.contains("bassMoodKnobs"));
  CHECK_EQ(b.extra.value("uiOnly", 0), 42);
  // Save and reopen: byte for byte the same.
  CHECK_EQ(canon(parseDocument(toJson(d))), canon(d));
  // None of it goes to the engine.
  const json p = toProjection(d);
  for (const auto& t : p["tracks"]) {
    CHECK_EQ(t.value("instrument", std::string()), std::string("none"));
    CHECK(!t.contains("modulators"));
  }
  const core::Project proj = core::fromJson(p);
  CHECK_EQ(proj.tracks.size(), (size_t)3);
}

TEST("legacy guard L2: the live effect and instrument paths still work") {
  Document d = parseDocument(legacySongJson());
  const std::string kickId = d.tracks[0].id;
  // addEffectWith/addPhraseEndComb are gone; the live path writes the same built-ins and automation knobs.
  const int at = addEffect(d, kickId, "filter");
  CHECK(at >= 0);
  setEffectParam(d, kickId, at, "mode", 8);  // Comb
  setEffectParam(d, kickId, at, "cutoff", 1200);
  const json& fx = effectChain(d, kickId)[(size_t)at];
  CHECK_EQ(fx.value("name", std::string()), std::string("filter"));
  CHECK_NEAR(effectParam(fx, "cutoff", 0), 1200, 1e-9);
  CHECK(!effectPresets().empty());
  const int preset = addEffect(d, kickId, effectPresets().front());
  CHECK(preset > at);
  // The instrument catalogue and the live parameter setter stay.
  const InstrumentInfo* info = findInstrumentInfo("kick");
  CHECK(info != nullptr);
  CHECK(!instrumentCatalogue().empty());
  if (info && !info->params.empty()) {
    setInstrumentParam(d, kickId, info->params[0].name, info->params[0].def);
    CHECK_NEAR(instrumentParamValue(d.tracks[0], info->params[0]), info->params[0].def, 1e-12);
  }
  // A sampler zone's loop data is still read (setSampleLoop is gone; the legacy zone keeps its loop).
  const Track& hats = d.tracks[2];
  CHECK_EQ(hats.instrument.samples.size(), (size_t)1);
  if (!hats.instrument.samples.empty()) {
    CHECK_NEAR(hats.instrument.samples[0].loopStart, 0.1, 1e-12);
    CHECK_NEAR(hats.instrument.samples[0].loopEnd, 0.4, 1e-12);
  }
}

TEST("legacy guard L2: the pattern catalogue loads and the kick/bass/cymbal alignment keeps its rules") {
  const PCoreCatalog c = loadPCoreCatalog(CORE_PCORE_CATALOG);
  CHECK_EQ(c.styles.size(), (size_t)7);
  CHECK_EQ(c.aligns.size(), (size_t)7);
  for (const auto& s : c.styles)
    if (!s.align.empty()) CHECK(c.align(s.align) != nullptr);
  const PCoreAlign* a = c.align("fullon");
  CHECK(a != nullptr);
  if (!a) return;
  PCoreLayers l;
  l.align = "fullon";
  CHECK(pcoreAlignFor(c, l) == a);
  l.align = "";
  CHECK(pcoreAlignFor(c, l) == nullptr);
  l.align = "no-such-align";
  bool threw = false;
  try { pcoreAlignFor(c, l); } catch (const DocumentError&) { threw = true; }
  CHECK(threw);

  const double bpm = 145.0;
  const int64_t bar = 4 * kPpq, end = bar;
  const std::vector<int64_t> kicks = {0, kPpq, 2 * kPpq, 3 * kPpq};
  auto note = [](int64_t tick, int64_t length, double velocity, int pitch) {
    Note n;
    n.tick = tick;
    n.length = length;
    n.velocity = velocity;
    n.pitch = pitch;
    return n;
  };
  Pattern kick, bass, hats, cymbals, lead;
  kick.lengthTicks = bass.lengthTicks = hats.lengthTicks = cymbals.lengthTicks = lead.lengthTicks = bar;
  for (int64_t t : kicks) kick.notes.push_back(note(t, 240, 0.9, 36));
  // Bass under the kicks (must go), offbeat notes too long (must shorten), the first after each kick loud (must soften).
  bass.notes = {note(0, 900, 1.0, 36),    note(480, 900, 1.0, 36),  note(720, 900, 0.9, 36),
                note(kPpq, 900, 1.0, 36), note(1440, 900, 1.0, 36), note(1680, 900, 0.9, 36),
                note(2400, 900, 0.9, 36)};
  for (int64_t t = 0; t < bar; t += kPpq / 4) hats.notes.push_back(note(t, 120, 0.8, 42));
  cymbals.notes.push_back(note(0, 960, 0.9, 49));
  lead.notes = {note(0, 480, 0.7, 60), note(720, 240, 0.6, 62)};
  const json kickBefore = notesJson(kick);
  const json leadBefore = notesJson(lead);
  std::vector<PCoreAlignTrack> tracks = {{&kick, "kick", {}}, {&bass, "bass", kicks}, {&hats, "hats", kicks},
                                         {&cymbals, "cymbals", kicks}, {&lead, "lead", kicks}};
  PCoreAlignStats st;
  pcoreAlignNotes(*a, bpm, 0, end, tracks, st);
  // The kick stays on its grid; a part outside the alignment is left alone.
  CHECK_EQ(notesJson(kick).dump(), kickBefore.dump());
  CHECK_EQ(notesJson(lead).dump(), leadBefore.dump());
  // No bass under a kick (nor in the steps kept free after it); every note ends bassTailMs before the next kick
  // and lasts at most bassGate of the space to the next note or kick.
  const int64_t clear = (int64_t)a->bassClearSteps * (kPpq / 4);
  const int64_t tail = (int64_t)std::lround(a->bassTailMs * bpm / 60.0 * kPpq / 1000.0);
  CHECK_EQ(bass.notes.size(), (size_t)5);
  for (const auto& n : bass.notes) {
    for (int64_t k : kicks) CHECK(!(n.tick >= k - 20 && n.tick <= k + 20 + clear));
    int64_t nextBass = end;
    for (const auto& m : bass.notes)
      if (m.tick > n.tick) { nextBass = m.tick; break; }
    const auto kk = std::upper_bound(kicks.begin(), kicks.end(), n.tick);
    const int64_t nextKick = kk != kicks.end() ? *kk : std::numeric_limits<int64_t>::max();
    if (nextKick != std::numeric_limits<int64_t>::max()) {
      CHECK(n.tick + n.length <= nextKick - tail);
      CHECK(n.length <= (int64_t)std::floor((double)(std::min(nextBass, nextKick) - n.tick) * a->bassGate));
    }
  }
  // The first bass note after a kick is softer than the others of its group (80 / 100 / 100).
  for (const auto& n : bass.notes)
    if (n.tick == 480 || n.tick == 1440) CHECK(n.velocity <= a->bassFirst * 0.9 + 1e-9);
  // Hats: on the kick softer, the offbeat hats on top, the rest under them; none gone (hatsOnKick > 0).
  CHECK_EQ(hats.notes.size(), (size_t)16);
  for (const auto& n : hats.notes) {
    const bool onKick = std::find(kicks.begin(), kicks.end(), n.tick) != kicks.end();
    if (onKick) CHECK(n.velocity <= 0.8 * a->hatsOnKick + 1e-9);
    else if (n.tick % (kPpq / 2) == kPpq / 4) CHECK_NEAR(n.velocity, 0.8, 1e-9);
    else CHECK(n.velocity <= 0.8 * (1.0 - a->hatsAccent) + 1e-9);
  }
  // The cymbal keeps its note and loudness (only grid, swing and push apply; fullon has neither).
  CHECK_EQ(cymbals.notes.size(), (size_t)1);
  if (!cymbals.notes.empty()) {
    CHECK_EQ(cymbals.notes[0].tick, (int64_t)0);
    CHECK_NEAR(cymbals.notes[0].velocity, 0.9, 1e-9);
  }
  // Applied twice gives the same notes (the catalogue's promise), and changes nothing the second time.
  const json once = json{{"kick", notesJson(kick)}, {"bass", notesJson(bass)}, {"hats", notesJson(hats)},
                         {"cymbals", notesJson(cymbals)}, {"lead", notesJson(lead)}};
  PCoreAlignStats st2;
  pcoreAlignNotes(*a, bpm, 0, end, tracks, st2);
  const json twice = json{{"kick", notesJson(kick)}, {"bass", notesJson(bass)}, {"hats", notesJson(hats)},
                          {"cymbals", notesJson(cymbals)}, {"lead", notesJson(lead)}};
  CHECK_EQ(once.dump(), twice.dump());
  CHECK_EQ(st2.moved, 0);
  CHECK_EQ(st2.removed, 0);
  CHECK_EQ(st2.shortened, 0);
  CHECK_EQ(st2.softer, 0);
}

TEST("legacy guard L2: the sample role catalogue and the live fit rules are unchanged") {
  // browseSampleRoles is gone; the sampler's own role list stays, in the settings' order.
  const std::vector<std::string> roles = {"kick", "bass", "hat", "openhat", "accent", "perc", "impact", "riser"};
  CHECK(samplerRoles() == roles);
  Track t;
  t.id = "kick"; t.name = "Kick"; CHECK_EQ(trackSampleRole(t), std::string("kick"));
  t.id = "bass"; t.name = "Bass"; CHECK_EQ(trackSampleRole(t), std::string("bass"));
  t.id = "open-hats"; t.name = "Open hats"; CHECK_EQ(trackSampleRole(t), std::string("openhat"));
  t.id = "hats"; t.name = "Hats"; CHECK_EQ(trackSampleRole(t), std::string("hat"));
  t.id = "crash"; t.name = "Crash"; CHECK_EQ(trackSampleRole(t), std::string("impact"));
  t.id = "lead"; t.name = "Lead"; CHECK_EQ(trackSampleRole(t), std::string(""));
  // sampleFitInRegion is gone; the live fit still holds a kick's tail before the next beat, a hat's before half a beat.
  LibrarySample s;
  s.sha256 = std::string(64, 'b');
  s.info.role = "kick";
  SampleAnalysis an;
  an.sha256 = s.sha256;
  an.complete = true;
  an.attackMs = 5;
  an.tailMs = 100;
  an.hits = 1;
  s.analysis = analysisToJson(an);
  SampleFit f = sampleFit(s, "kick", 145.0);
  CHECK(f.known && f.fits);
  an.tailMs = 500;
  s.analysis = analysisToJson(an);
  f = sampleFit(s, "kick", 145.0);
  CHECK(f.known && !f.fits);
  s.info.role = "hat";
  an.tailMs = 100;
  s.analysis = analysisToJson(an);
  f = sampleFit(s, "hat", 145.0);
  CHECK(f.known && f.fits);
  an.tailMs = 300;
  s.analysis = analysisToJson(an);
  f = sampleFit(s, "hat", 145.0);
  CHECK(f.known && !f.fits);
  s.info.role = "kick";
  s.analysis = json();
  f = sampleFit(s, "kick", 145.0);
  CHECK(!f.known);
  // A bass one-shot: the role is enough (cut at note-off). Kinds with no track name of their own still fit.
  s.info.role = "bass";
  f = sampleFit(s, "bass", 145.0);
  CHECK(f.fits);
  s.info.role = "fx";
  f = sampleFit(s, "fx", 145.0);
  CHECK(f.fits);
}

int main(int argc, char** argv) { return coretest::runAll(argc, argv); }