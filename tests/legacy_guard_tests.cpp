// Characterization guards for the dead-code removal of lots L1/L2 (2026-11-18, dead-code inventory).
// These pin the LIVE behavior the removals must not change: old songs with Core's own instruments,
// sampler zones, synth data, modulators and ducking still open, save and reopen byte for byte, and
// stay out of the engine projection; the five visible templates still open and render; a legacy
// engine project still loads and renders, bit for bit, at any block size.
// Run: legacy_guard_tests [part-of-name]
#include <algorithm>
#include <cmath>
#include <filesystem>
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

int main(int argc, char** argv) { return coretest::runAll(argc, argv); }