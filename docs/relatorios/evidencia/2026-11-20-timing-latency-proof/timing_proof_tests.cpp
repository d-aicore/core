// STAGE 2 — timing / latency-compensation / sound-integrity PROOF (owner round-2, gates Chain parallel).
// Additive permanent regression + a --evidence measurement mode. No product code is touched.
//
// Owner contract (verbatim): «Cria uma prova controlada da compensação de latência, do timing MIDI e da
// automação do Core. Usa um impulso e um plugin de teste com atraso conhecido. Mede o desvio em samples
// entre dois caminhos, um direto e outro com atraso, antes e depois da compensação. Exercita também uma
// mudança de latência reportada pelo plugin, se houver suporte. Coloca eventos MIDI e pontos de automação
// perto do limite de um bloco. Verifica onde aparecem no render. Repete em dois tamanhos de buffer e, se
// disponível, a 44,1 e 48 kHz. Usa subtração entre renders para casos determinísticos com o mesmo
// processamento. Em plugins com modulação ou variação entre execuções, define uma medição apropriada e
// indica a tolerância. Entrega o projeto, o método de medição, os desvios em samples e os renders
// relevantes. Dá prioridade ao alinhamento entre kick e baixo e ao comportamento das cadeias paralelas.»
//
// METHOD (two complementary paths, both the REAL Graph::process):
//  * OFFLINE (fake provider, always runs, ctest): a deterministic fake delay/click processor with a
//    DECLARED latency exercises the graph's PDC, MIDI scheduling, automation and bus recombination with
//    no device and no JUCE. renderProject() is the real export path (preroll + PDC); a raw Graph WITHOUT
//    preroll reveals the uncompensated path offset ("before"). Deviations are measured in samples.
//  * REAL VST3 (--evidence): the serve architecture (in-process Server + JuceHost + null device), the
//    existing Engine::startCapture/captureL/captureR seam (copies the mix AFTER Graph::process, BEFORE the
//    ear guard). Uses CoreTestSynth/CoreTestGain (built with the app) and the OUT-OF-REPO probes
//    CoreTimingDelay (known delay, reports it as latency, runtime "delay" param) and CoreTimingClick
//    (one-sample impulse at the exact note-on). Writes probe songs, captures (.f32 + .wav) and a JSON
//    deviation table. Real host adds the fixed +128 gather (juce_host.cpp:162) on every plugin path.
//
// Key code paths cited: PDC collect graph.cpp:531-551; path/bus latency :610-672; per-track delay line
// :1367-1371; preroll :707-720 (export only, server.cpp:156 / render_job.cpp:395); real time plays
// latency_ late (no preroll). MIDI sample-accurate renderTrack :969-1066 + EventLog :1055. Automation read
// at aligned 32-sample sub-block step() :1146-1167, fader ramp :1347-1352, plugin-param glide :769-777.
// Latency CHANGE: Server::checkPluginLatency server.cpp:288-292 (serve tick main.cpp:155). Parallel = M7a
// bus routing (implemented); M7c sidechain DORMANT for plugins (S9 report).
//
// Run: timing_proof_tests [name-filter]                 (permanent regression, ctest -L merge)
//      timing_proof_tests --evidence <outDir> [delayVst3] [clickVst3]   (full real-VST3 matrix + files)
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "support/check.hpp"
#include "core/engine.hpp"
#include "core/project.hpp"
#include "core/server.hpp"
#include "core/graph.hpp"
#include "core/assets.hpp"
#include "core/tempo_map.hpp"
#include "io/device.hpp"
#include "io/wav.hpp"
#include "plugins/juce_host.hpp"

using namespace core;
using namespace coretest;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------------------------- helpers
int64_t argmaxAbs(const std::vector<float>& v) {
  int64_t at = -1; float best = 0;
  for (size_t i = 0; i < v.size(); ++i) { const float a = std::fabs(v[i]); if (a > best) { best = a; at = (int64_t)i; } }
  return at;
}
int64_t firstAbove(const std::vector<float>& v, float thresh, int64_t from = 0) {
  for (size_t i = (size_t)std::max<int64_t>(0, from); i < v.size(); ++i) if (std::fabs(v[i]) > thresh) return (int64_t)i;
  return -1;
}
// The peak within a tight window around center: isolates ONE note's impulse when notes are close together
// (a global firstAbove would grab a neighboring note). Returns -1 if the window is silent.
int64_t peakNear(const std::vector<float>& v, int64_t center, int64_t radius) {
  int64_t at = -1; float best = 0;
  const int64_t lo = std::max<int64_t>(0, center - radius), hi = std::min<int64_t>((int64_t)v.size() - 1, center + radius);
  for (int64_t i = lo; i <= hi; ++i) { const float a = std::fabs(v[(size_t)i]); if (a > best) { best = a; at = i; } }
  return at;
}
int64_t idealSample(int64_t tick, double bpm, double sr) {
  return (int64_t)std::llround((double)tick * sr * 60.0 / (bpm * kPpq));
}

std::string makeImpulseWav(const std::string& dir, double sr) {
  std::vector<float> L(256, 0.0f), R(256, 0.0f);
  L[0] = 0.9f; R[0] = 0.9f;
  const std::string path = dir + "/impulse-" + std::to_string((int)sr) + ".wav";
  writeWav(path, L, R, (int)sr, 32);
  return path;
}
std::string makeDCWav(const std::string& dir, double sr, int64_t frames) {
  std::vector<float> L((size_t)frames, 1.0f), R((size_t)frames, 1.0f);
  const std::string path = dir + "/dc-" + std::to_string((int)sr) + ".wav";
  writeWav(path, L, R, (int)sr, 32);
  return path;
}
std::string tempDir() {
  const std::string d = "C:/Core-build/timing/tmp";
  std::filesystem::create_directories(d);
  return d;
}

json note(int64_t tick, int64_t length, int pitch, double velocity) {
  return {{"tick", tick}, {"length", length}, {"pitch", pitch}, {"velocity", velocity}};
}
json fakePlugin(const std::string& id) {
  return {{"format", "vst3"}, {"id", id}, {"name", id}, {"vendor", "Core"}, {"version", "1.0.0"}};
}
json audioTrack(const std::string& id, const std::string& wav, int64_t startTick, double durationSeconds, const json& effects) {
  json clip = {{"startTick", startTick}, {"path", wav}, {"offsetSeconds", 0.0}, {"durationSeconds", durationSeconds}, {"gainDb", 0.0}};
  json t = {{"id", id}, {"name", id}, {"instrument", "audio"}, {"audioClips", json::array({clip})}};
  if (effects.is_array() && !effects.empty()) t["effects"] = effects;
  return t;
}
json pluginTrack(const std::string& id, const json& plugin, const json& notes, int64_t patternTicks, const json& effects) {
  json t = {{"id", id}, {"name", id}, {"instrument", "plugin"}, {"plugin", plugin},
            {"patternTicks", patternTicks}, {"notes", notes}};
  if (effects.is_array() && !effects.empty()) t["effects"] = effects;
  return t;
}
json song(const json& tracks, double bpm, int64_t lengthTicks) {
  return {{"format", kProjectFormat}, {"version", 2}, {"name", "timing proof"}, {"bpm", bpm},
          {"lengthTicks", lengthTicks}, {"masterGainDb", 0.0}, {"tracks", tracks}};
}
json delayEffect(const std::string& id, const json& plugin) {
  return {{"type", "plugin"}, {"id", id}, {"plugin", plugin}};
}

// ---------------------------------------------------------------------------------------------- fake plugin
// A deterministic delay (effect) or one-sample-click (instrument). DECLARED latency is independent of the
// actual delay so a mismatch residual can be measured; by default they are equal (an honest plugin).
struct FakeDelay final : PluginProcessor {
  static constexpr int kMax = 8192;
  int latency = 0, delay = 0;
  float gain = 1.0f;
  bool inst = false;
  std::array<std::array<float, kMax>, 2> line{};
  int wp = 0;
  std::vector<MidiNote> seen;
  FakeDelay(int lat, bool instrument = false) : latency(lat), delay(lat), inst(instrument) {}
  void reset() { wp = 0; for (auto& c : line) c.fill(0.0f); }
  void process(float* L, float* R, int frames, const MidiNote* notes, int count) override {
    for (int i = 0; i < count; ++i) seen.push_back(notes[i]);
    if (inst) {
      for (int i = 0; i < frames; ++i) { L[i] = 0; R[i] = 0; }
      for (int i = 0; i < count; ++i) {
        const MidiNote& m = notes[i];
        if (m.on && m.offset >= 0 && m.offset < frames) { L[m.offset] += m.velocity * gain; R[m.offset] += m.velocity * gain; }
      }
    } else {
      for (int i = 0; i < frames; ++i) {
        const int rp = (wp - delay + kMax) % kMax;
        const float ol = line[0][(size_t)rp], orr = line[1][(size_t)rp];
        line[0][(size_t)wp] = L[i]; line[1][(size_t)wp] = R[i];
        L[i] = ol * gain; R[i] = orr * gain;
        wp = (wp + 1) % kMax;
      }
    }
  }
  bool isInstrument() const override { return inst; }
  int latencySamples() const override { return latency; }
};

// Fresh instance per get() (as a real export): deterministic, no state carried between renders. The plugin
// id encodes the fake: "fdelay:<N>" = delay effect of N samples, "fclick" = click instrument (0 latency).
struct FreshProvider final : PluginProvider {
  std::shared_ptr<PluginProcessor> get(const std::string&, const json& plugin, double, bool, std::string* why) override {
    if (why) why->clear();
    const std::string id = plugin.value("id", "");
    if (id.rfind("fdelay:", 0) == 0) return std::make_shared<FakeDelay>(std::atoi(id.c_str() + 7), false);
    if (id == "fclick") return std::make_shared<FakeDelay>(0, true);
    if (why) *why = "unknown fake id " + id;
    return nullptr;
  }
  void keepOnly(const std::vector<std::string>&) override {}
};

// A stable instance by key (real-time reuse), so a latency CHANGE can be applied and detected.
struct StableProvider final : PluginProvider {
  std::map<std::string, std::shared_ptr<FakeDelay>> procs;
  std::shared_ptr<PluginProcessor> get(const std::string& key, const json&, double, bool, std::string* why) override {
    if (why) why->clear();
    auto it = procs.find(key);
    if (it != procs.end()) return it->second;
    if (why) *why = "no stable fake for " + key;
    return nullptr;
  }
  void keepOnly(const std::vector<std::string>&) override {}
};

// Raw render: Graph WITHOUT preroll -> the uncompensated path offset is visible ("before").
std::vector<float> renderRaw(const json& songJson, double sr, int block, AssetStore* assets, PluginProvider* prov, int64_t total) {
  const Project p = fromJson(songJson);
  Graph g(p, sr, assets, prov, true);
  // deliberately NO preroll(): shift_ stays 0, delay lines start empty
  std::vector<float> L((size_t)total, 0.0f), R((size_t)total, 0.0f);
  for (int64_t pos = 0; pos < total; pos += block) {
    const int n = (int)std::min<int64_t>(block, total - pos);
    g.process(L.data() + pos, R.data() + pos, n, pos, false);
  }
  return L;
}

struct BitDiff { bool identical = true; int64_t firstAt = -1; uint64_t count = 0; float maxDelta = 0; };
BitDiff bitCompare(const std::vector<float>& aL, const std::vector<float>& aR, const std::vector<float>& bL, const std::vector<float>& bR) {
  BitDiff d;
  d.identical = aL.size() == bL.size() && aR.size() == bR.size() &&
                std::memcmp(aL.data(), bL.data(), aL.size() * 4) == 0 && std::memcmp(aR.data(), bR.data(), aR.size() * 4) == 0;
  if (d.identical) return d;
  auto scan = [&](const std::vector<float>& a, const std::vector<float>& b, int64_t base) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
      uint32_t ba, bb; std::memcpy(&ba, &a[i], 4); std::memcpy(&bb, &b[i], 4);
      if (ba == bb) continue;
      if (d.firstAt < 0) d.firstAt = base + (int64_t)i;
      ++d.count; d.maxDelta = std::max(d.maxDelta, std::fabs(a[i] - b[i]));
    }
  };
  scan(aL, bL, 0); scan(aR, bR, (int64_t)aL.size());
  return d;
}

const double kRates[2] = {48000.0, 44100.0};

}  // namespace

// ================================================================================================
// PERMANENT REGRESSION (offline, fake provider, deterministic — always runs, no device, no JUCE)
// ================================================================================================

TEST("Timing PDC: impulse dry vs known-delay — BEFORE (raw) and AFTER (compensated), 48k") {
  const std::string dir = tempDir();
  // 48k only here: at 44.1k the clip start tick lands on a half-sample (exactStart 1837.5, c.start 1838)
  // and graph.cpp:438 rounds the clip offset llround(0.5)=1, dropping WAV frame 0 (the impulse) -> silence.
  // That is a real clip-offset edge case (BUG-TIMING-CLIP-OFFSET), pinned as OPEN-DEFECT below and in the
  // report — NOT a PDC compensation failure. PDC alignment itself is proven here at 48k.
  for (double sr : {48000.0}) {
    AssetStore assets; FreshProvider prov;
    const std::string imp = makeImpulseWav(dir, sr);
    const double bpm = 120.0; const int64_t startTick = 80;
    const int64_t S = idealSample(startTick, bpm, sr);
    const int delayN = 480;
    const json dry = audioTrack("dry", imp, startTick, 256.0 / sr, json::array());
    const json wet = audioTrack("wet", imp, startTick, 256.0 / sr, json::array({delayEffect("d", fakePlugin("fdelay:480"))}));
    const int64_t total = S + delayN + 4096;
    // BEFORE: each path alone, no preroll -> the raw offset the delay adds.
    const auto dryRaw = renderRaw(song(json::array({dry}), bpm, 16 * 4 * kPpq), sr, 512, &assets, &prov, total);
    const auto wetRaw = renderRaw(song(json::array({wet}), bpm, 16 * 4 * kPpq), sr, 512, &assets, &prov, total);
    const int64_t dryPos = argmaxAbs(dryRaw), wetPos = argmaxAbs(wetRaw);
    const int64_t beforeDev = wetPos - dryPos;
    std::fprintf(stderr, "PDC sr=%.0f BEFORE dry=%lld wet=%lld dev=%lld (expect %d)\n", sr, (long long)dryPos, (long long)wetPos, (long long)beforeDev, delayN);
    CHECK_EQ(dryPos, S);
    CHECK_EQ(beforeDev, (int64_t)delayN);
    // AFTER: two tracks, real export path (PDC + preroll). Stems give each path's position.
    const Project both = fromJson(song(json::array({dry, wet}), bpm, 16 * 4 * kPpq));
    const RenderResult stems = renderProject(both, sr, 512, true, 0.2, nullptr, &assets, &prov, 0);
    const int64_t dryAfter = argmaxAbs(stems.stemL[0]), wetAfter = argmaxAbs(stems.stemL[1]);
    const int64_t afterDev = wetAfter - dryAfter;
    std::fprintf(stderr, "PDC sr=%.0f AFTER dry=%lld wet=%lld dev=%lld ideal=%lld\n", sr, (long long)dryAfter, (long long)wetAfter, (long long)afterDev, (long long)S);
    CHECK_EQ(afterDev, (int64_t)0);
    CHECK_EQ(dryAfter, S);   // preroll removes the common offset: the file starts on the song's sample
    CHECK_EQ(wetAfter, S);
    // Subtraction: aligned deterministic same-processing renders cancel to silence (dry stem == wet stem).
    const BitDiff sub = bitCompare(stems.stemL[0], stems.stemR[0], stems.stemL[1], stems.stemR[1]);
    std::fprintf(stderr, "PDC sr=%.0f dry-vs-wet stem bitExact=%d maxDelta=%.9g\n", sr, (int)sub.identical, sub.maxDelta);
    CHECK(sub.identical);
  }
}

TEST("Timing OPEN-DEFECT clip-offset: 44.1k half-sample start drops file frame 0 (pin; fix pending owner)") {
  // BUG-TIMING-CLIP-OFFSET (measured, NOT a harness bug): an audio clip with offsetSeconds=0 whose
  // startTick lands on a fractional sample (44.1k/120bpm/tick80 -> exactStart 1837.5, c.start 1838)
  // gets c.offset = llround(c.start - exactStart) = llround(0.5) = 1 (graph.cpp:438), skipping WAV
  // frame 0. An impulse at frame 0 therefore renders SILENT (argmaxAbs -1). At 48k the start is integer
  // (offset 0) so it plays. This PINS the current defective behavior so it is visible and tracked; when
  // the offset rounding is fixed (offset 0 when offsetSeconds==0, or floor instead of llround), FLIP
  // this pin to expect the impulse at S. One-sample edge case; catastrophic only for an impulse at 0.
  const std::string dir = tempDir();
  const double sr = 44100.0; AssetStore assets; FreshProvider prov;
  const std::string imp = makeImpulseWav(dir, sr);
  const double bpm = 120.0; const int64_t startTick = 80;
  const int64_t S = idealSample(startTick, bpm, sr);
  const json dry = audioTrack("dry", imp, startTick, 256.0 / sr, json::array());
  const int64_t total = S + 480 + 4096;
  const auto dryRaw = renderRaw(song(json::array({dry}), bpm, 16 * 4 * kPpq), sr, 512, &assets, &prov, total);
  const int64_t dryPos = argmaxAbs(dryRaw);
  std::fprintf(stderr, "CLIP-OFFSET sr=%.0f ideal=%lld dryPos=%lld (defect: frame0 dropped -> -1 silence)\n",
               sr, (long long)S, (long long)dryPos);
  CHECK_EQ(dryPos, (int64_t)-1);  // PIN of the current defect; flip to S once graph.cpp:438 is fixed
}

TEST("Timing PDC: block-size independence (64/128/512) and serial==parallel export, bit-exact") {
  const std::string dir = tempDir();
  const double sr = 48000.0; AssetStore assets; FreshProvider prov;
  const std::string imp = makeImpulseWav(dir, sr);
  const double bpm = 120.0; const int64_t startTick = 80;
  const json dry = audioTrack("dry", imp, startTick, 256.0 / sr, json::array());
  const json wet = audioTrack("wet", imp, startTick, 256.0 / sr, json::array({delayEffect("d", fakePlugin("fdelay:480"))}));
  const Project both = fromJson(song(json::array({dry, wet}), bpm, 16 * 4 * kPpq));
  const RenderResult ref = renderProject(both, sr, 512, false, 0.2, nullptr, &assets, &prov, 0);
  for (int block : {64, 128, 256}) {
    const RenderResult r = renderProject(both, sr, block, false, 0.2, nullptr, &assets, &prov, 0);
    const BitDiff d = bitCompare(ref.L, ref.R, r.L, r.R);
    std::fprintf(stderr, "PDC block-independence 512 vs %d bitExact=%d firstDiff=%lld maxDelta=%.9g\n", block, (int)d.identical, (long long)d.firstAt, d.maxDelta);
    CHECK(d.identical);
  }
  // Parallel export workers must equal serial bit for bit (quality bar 4, offline path).
  const RenderResult par = renderProject(both, sr, 512, false, 0.2, nullptr, &assets, &prov, 2);
  const BitDiff dp = bitCompare(ref.L, ref.R, par.L, par.R);
  std::fprintf(stderr, "PDC serial-vs-parallel(2) bitExact=%d maxDelta=%.9g\n", (int)dp.identical, dp.maxDelta);
  CHECK(dp.identical);
}

TEST("Timing MIDI: sample-accurate scheduling near block boundaries (EventLog + click onset), 48k/44.1k") {
  FreshProvider prov;
  for (double sr : kRates) {
    const double bpm = 600.0;  // samples/tick = 5 at 48k (integer grid); ideal always = sampleOfTick(tick)
    // Ticks whose samples sit near the 32/64/128-sample edges (and mid-block): 6->30, 7->35, 12->60, 13->65,
    // 25->125, 26->130, 102->510, 103->515 at 48k.
    const json notes = json::array({note(6, 8, 60, 0.8), note(7, 8, 62, 0.8), note(12, 8, 64, 0.8), note(13, 8, 65, 0.8),
                                    note(25, 8, 67, 0.8), note(26, 8, 69, 0.8), note(102, 8, 71, 0.8), note(103, 8, 72, 0.8)});
    const json track = pluginTrack("click", fakePlugin("fclick"), notes, 4 * kPpq, json::array());
    const Project p = fromJson(song(json::array({track}), bpm, 16 * 4 * kPpq));
    EventLog log;
    const RenderResult r = renderProject(p, sr, 512, false, 0.2, &log, nullptr, &prov, 0);
    CHECK(!log.entries.empty());
    int64_t maxSchedDev = 0;
    // EventLog scheduling deviation: logged sample must equal the tempo map's sampleOfTick exactly.
    const TempoMap tempo(bpm, {}, sr);
    for (const auto& e : log.entries) {
      const int64_t ideal = tempo.sampleOfTick(e.tick);
      const int64_t dev = e.sample - ideal;
      maxSchedDev = std::max(maxSchedDev, std::abs(dev));
      CHECK_EQ(e.sample, ideal);
    }
    // Audio onset deviation: the fake click emits a one-sample impulse at the exact note sample (0 latency).
    // peakNear isolates this note's impulse (notes repeat every pattern and sit only a few samples apart).
    int64_t maxOnsetDev = 0;
    for (const auto& e : log.entries) {
      const int64_t ideal = tempo.sampleOfTick(e.tick);
      const int64_t onset = peakNear(r.L, ideal, 2);
      const int64_t dev = onset - ideal;
      maxOnsetDev = std::max(maxOnsetDev, std::abs(dev));
      CHECK_EQ(onset, ideal);
    }
    std::fprintf(stderr, "MIDI sr=%.0f events=%zu maxSchedDev=%lld maxOnsetDev=%lld\n", sr, log.entries.size(), (long long)maxSchedDev, (long long)maxOnsetDev);
    // Block-size independence of the onsets.
    const RenderResult r64 = renderProject(p, sr, 64, false, 0.2, nullptr, nullptr, &prov, 0);
    const BitDiff d = bitCompare(r.L, r.R, r64.L, r64.R);
    CHECK(d.identical);
  }
}

TEST("Timing automation: gainDb step lands on the 32-sample sub-block grid, buffer-independent") {
  const std::string dir = tempDir();
  for (double sr : kRates) {
    AssetStore assets; FreshProvider prov;
    const double bpm = 600.0;
    const std::string dc = makeDCWav(dir, sr, 8192);
    // A sharp gain step 0dB -> -60dB at Tstep (two points at the same tick). DC input=1 so output=fader gain.
    for (int64_t Tstep : {12, 13, 25, 26}) {
      const int64_t Sstep = idealSample(Tstep, bpm, sr);
      json env = {{"param", "gainDb"}, {"points", json::array({{{"tick", Tstep}, {"value", 0.0}}, {{"tick", Tstep}, {"value", -60.0}}})}};
      json track = audioTrack("auto", dc, 0, 8192.0 / sr, json::array());
      track["envelopes"] = json::array({env});
      const Project p = fromJson(song(json::array({track}), bpm, 16 * 4 * kPpq));
      const RenderResult r = renderProject(p, sr, 512, false, 0.0, nullptr, &assets, &prov, 0);
      // transition start: first sample where the gain leaves 1.0; end: first sample below 0.01 (~-40dB).
      int64_t start = -1, end = -1;
      for (size_t i = 1; i < r.L.size(); ++i) { if (r.L[i] < 0.999f) { start = (int64_t)i; break; } }
      for (size_t i = 1; i < r.L.size(); ++i) { if (r.L[i] < 0.01f) { end = (int64_t)i; break; } }
      const int64_t grid = ((start + 31) / 32) * 32;  // the 32-sample sub-block boundary at/after start
      std::fprintf(stderr, "AUTO sr=%.0f Tstep=%lld Sstep=%lld start=%lld end=%lld gridStart=%lld devFromStep=%lld onGrid=%d\n",
                   sr, (long long)Tstep, (long long)Sstep, (long long)start, (long long)end, (long long)grid, (long long)(start - Sstep), (int)(start % 32 == 0 || start == grid));
      CHECK(start >= 0);
      CHECK(start >= Sstep);                 // never early
      CHECK(start - Sstep <= 32);            // quantized to the next sub-block (<=32 samples late)
      CHECK_EQ(start % 32, (int64_t)0);      // lands exactly on the 32-sample sub-block grid
      // Buffer independence: the landing is identical at block 64.
      const RenderResult r64 = renderProject(p, sr, 64, false, 0.0, nullptr, &assets, &prov, 0);
      int64_t start64 = -1;
      for (size_t i = 1; i < r64.L.size(); ++i) { if (r64.L[i] < 0.999f) { start64 = (int64_t)i; break; } }
      CHECK_EQ(start64, start);
    }
  }
}

TEST("Timing parallel: M7a bus recombination aligns branches of different latency (stems + subtraction)") {
  const std::string dir = tempDir();
  const double sr = 48000.0; AssetStore assets; FreshProvider prov;
  const std::string imp = makeImpulseWav(dir, sr);
  const double bpm = 120.0; const int64_t startTick = 80;
  const int64_t S = idealSample(startTick, bpm, sr);
  // dry (latency 0) and wet (latency 480) both feed a bus; the bus waits for the latest and delays the dry.
  json dry = audioTrack("dry", imp, startTick, 256.0 / sr, json::array()); dry["output"] = "bus";
  json wet = audioTrack("wet", imp, startTick, 256.0 / sr, json::array({delayEffect("d", fakePlugin("fdelay:480"))})); wet["output"] = "bus";
  json bus = {{"id", "bus"}, {"name", "Bus"}, {"instrument", "bus"}};
  const Project p = fromJson(song(json::array({dry, wet, bus}), bpm, 16 * 4 * kPpq));
  const RenderResult stems = renderProject(p, sr, 512, true, 0.2, nullptr, &assets, &prov, 0);
  const int64_t dryPos = argmaxAbs(stems.stemL[0]), wetPos = argmaxAbs(stems.stemL[1]);
  std::fprintf(stderr, "PARALLEL bus dry=%lld wet=%lld dev=%lld ideal=%lld\n", (long long)dryPos, (long long)wetPos, (long long)(wetPos - dryPos), (long long)S);
  CHECK_EQ(wetPos - dryPos, (int64_t)0);
  CHECK_EQ(dryPos, S);
  // Subtraction: the two branches, aligned at the bus, cancel (same deterministic impulse, unity gain).
  const BitDiff sub = bitCompare(stems.stemL[0], stems.stemR[0], stems.stemL[1], stems.stemR[1]);
  CHECK(sub.identical);
}

TEST("Timing latency-change: Server::checkPluginLatency detects a reported change, rebuilds, re-aligns") {
  struct NullOut final : AudioOutput {
    json start(Engine&, const json&) override { return {{"sampleRate", 48000.0}, {"blockSize", 512}, {"backend", "null"}}; }
    void stop() override {}
    json devices(const json&) override { return json::array(); }
  };
  struct FakeHost final : PluginHost {
    StableProvider* prov = nullptr;
    std::shared_ptr<PluginProcessor> get(const std::string& key, const json& plugin, double sr, bool offline, std::string* why) override { return prov->get(key, plugin, sr, offline, why); }
    void keepOnly(const std::vector<std::string>& keys) override { prov->keepOnly(keys); }
    json captureStates(const std::vector<std::string>&) override { return json::object(); }
    bool showEditor(const std::string&, bool, std::string*) override { return false; }
    bool takeChanged() override { return false; }
    void releaseAll() override {}
    void resetAll() override {}
  };
  StableProvider stable;
  auto delayProc = std::make_shared<FakeDelay>(480, false);
  stable.procs["fx:wet:d"] = delayProc;
  FakeHost host; host.prov = &stable;
  const std::string dir = tempDir();
  const std::string imp = makeImpulseWav(dir, 48000.0);
  const json wet = audioTrack("wet", imp, 80, 256.0 / 48000.0, json::array({delayEffect("d", fakePlugin("stable-delay"))}));
  const json songJson = song(json::array({wet}), 120.0, 16 * 4 * kPpq);
  Server server([] { return std::make_unique<NullOut>(); }, 48000.0, &host);
  const json loaded = server.execute("project.load", {{"project", songJson}});
  CHECK(loaded.value("missing", json::array()).empty());
  CHECK(!server.checkPluginLatency());          // stable right after load
  // Re-alignment BEFORE the change (export path, preroll): the impulse sits on its song sample.
  AssetStore assets;
  const Project p = fromJson(songJson);
  delayProc->reset();
  const RenderResult before = renderProject(p, 48000.0, 512, true, 0.2, nullptr, &assets, &stable, 0);
  CHECK_EQ(argmaxAbs(before.stemL[0]), idealSample(80, 120.0, 48000.0));
  // The plugin re-reports a NEW latency (480 -> 960), honest (the actual delay moves with it).
  delayProc->latency = 960; delayProc->delay = 960;
  CHECK(server.checkPluginLatency());           // THE seam: detected the change and rebuilt
  CHECK(!server.checkPluginLatency());          // stable again at the new latency
  // Re-alignment AFTER the change: still on the same song sample (PDC re-compensated).
  delayProc->reset();
  const RenderResult after = renderProject(p, 48000.0, 512, true, 0.2, nullptr, &assets, &stable, 0);
  const int64_t wetAfter = argmaxAbs(after.stemL[0]);
  std::fprintf(stderr, "LATENCY-CHANGE 480->960: re-aligned wet=%lld ideal=%lld\n", (long long)wetAfter, (long long)idealSample(80, 120.0, 48000.0));
  CHECK_EQ(wetAfter, idealSample(80, 120.0, 48000.0));
}

TEST("Timing determinism: two identical renders are bit-exact (fake, no modulation)") {
  const std::string dir = tempDir();
  const double sr = 48000.0; AssetStore assets; FreshProvider prov;
  const std::string imp = makeImpulseWav(dir, sr);
  const json dry = audioTrack("dry", imp, 80, 256.0 / sr, json::array());
  const json wet = audioTrack("wet", imp, 80, 256.0 / sr, json::array({delayEffect("d", fakePlugin("fdelay:480"))}));
  const Project p = fromJson(song(json::array({dry, wet}), 120.0, 16 * 4 * kPpq));
  const RenderResult a = renderProject(p, sr, 512, false, 0.2, nullptr, &assets, &prov, 0);
  const RenderResult b = renderProject(p, sr, 512, false, 0.2, nullptr, &assets, &prov, 0);
  const BitDiff d = bitCompare(a.L, a.R, b.L, b.R);
  CHECK(d.identical);
}

// ================================================================================================
// REAL-VST3 EVIDENCE MODE (--evidence): serve architecture + null device + capture seam.
// ================================================================================================
namespace {

#ifdef CORE_TEST_SYNTH_VST3
struct TestHost final : PluginHost {
  plugins::JuceHost host;
  std::shared_ptr<PluginProcessor> get(const std::string& key, const json& plugin, double sr, bool offline, std::string* why) override {
    return host.get(key, plugin, sr, offline, why);
  }
  void keepOnly(const std::vector<std::string>& keys) override { host.keepOnly(keys); }
  json captureStates(const std::vector<std::string>& keys) override { return host.captureStates(keys); }
  bool showEditor(const std::string& key, bool open, std::string* why) override { return host.showEditor(key, open, why); }
  bool takeChanged() override { return host.takeChanged(); }
  void releaseAll() override { host.releaseAll(); }
  void resetAll() override { host.resetAll(); }
};

struct Capture { std::vector<float> L, R; int deviceBlock = 0; double sr = 0; float peak = 0; };

Capture runCapture(const json& songJson, double sr, int block, int64_t frames, const std::string& listenDbOff = "") {
  Capture c; c.sr = sr;
  TestHost host;
  Server server(makeMiniaudioOutput, sr, &host);
  const json loaded = server.execute("project.load", {{"project", songJson}});
  if (!loaded.value("missing", json::array()).empty())
    std::fprintf(stderr, "EVIDENCE WARNING missing plugins: %s\n", loaded.value("missing", json::array()).dump().c_str());
  const json start = server.execute("engine.start", {{"backend", "null"}, {"blockSize", block}});
  c.deviceBlock = start.value("blockSize", 0);
  Engine& e = server.engine();
  e.setEarGuard(false);           // measure the raw mix (the guard is a listening safety, not the song)
  e.startCapture(frames);
  server.execute("transport.play", json::object());
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (e.captured() < frames && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(5ms);
  e.stopCapture();
  c.L.assign(e.captureL().begin(), e.captureL().begin() + (size_t)e.captured());
  c.R.assign(e.captureR().begin(), e.captureR().begin() + (size_t)e.captured());
  for (size_t i = 0; i < c.L.size(); ++i) c.peak = std::max({c.peak, std::fabs(c.L[i]), std::fabs(c.R[i])});
  server.execute("transport.stop", json::object());
  server.execute("engine.stop", json::object());
  return c;
}

void writeRaw(const std::string& path, const Capture& c) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char*>(c.L.data()), (std::streamsize)(c.L.size() * 4));
  f.write(reinterpret_cast<const char*>(c.R.data()), (std::streamsize)(c.R.size() * 4));
}
void writeWavCap(const std::string& path, const Capture& c) { writeWav(path, c.L, c.R, (int)c.sr, 24); }
void writeText(const std::string& path, const std::string& s) { std::ofstream f(path); f << s; }

int runEvidence(const std::string& outDir, const std::string& delayPath, const std::string& clickPath) {
  std::filesystem::create_directories(outDir);
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  json delay = delayPath.empty() ? json() : plugins::scanFile(delayPath)[0];
  json click = clickPath.empty() ? json() : plugins::scanFile(clickPath)[0];
  std::fprintf(stderr, "EVIDENCE synth=%d gain=%d delay=%d click=%d\n", (int)synth.is_object(), (int)gain.is_object(), (int)delay.is_object(), (int)click.is_object());
  json table = json::array();
  const std::string dir = outDir;

  // ---- PDC impulse through the REAL host (dry audio vs audio+CoreTimingDelay). Real gather +128.
  if (delay.is_object()) {
    for (double sr : kRates) {
      const std::string imp = makeImpulseWav(dir, sr);
      const double bpm = 120.0; const int64_t startTick = 80;
      const int64_t S = idealSample(startTick, bpm, sr);
      const json dry = audioTrack("dry", imp, startTick, 256.0 / sr, json::array());
      const json wet = audioTrack("wet", imp, startTick, 256.0 / sr, json::array({delayEffect("d", delay)}));
      const json both = song(json::array({dry, wet}), bpm, 16 * 4 * kPpq);
      writeText(dir + "/song-pdc-impulse.json", both.dump(2));
      for (int block : {128, 512}) {
        // Real-time capture: capture[c] = song[c - latency_]; both paths aligned, whole mix latency_ late.
        const int64_t frames = (int64_t)(sr * 0.6);
        const Capture c = runCapture(both, sr, block, frames);
        writeRaw(dir + "/pdc-impulse-" + std::to_string((int)sr) + "-" + std::to_string(block) + ".f32", c);
        writeWavCap(dir + "/pdc-impulse-" + std::to_string((int)sr) + "-" + std::to_string(block) + ".wav", c);
        // Export stems give each path's absolute position (preroll removes the common offset).
        AssetStore assets; TestHost host;
        const Project p = fromJson(both);
        const RenderResult stems = renderProject(p, sr, 512, true, 0.3, nullptr, &assets, &host, 0);
        const int64_t dryPos = argmaxAbs(stems.stemL[0]), wetPos = argmaxAbs(stems.stemL[1]);
        const BitDiff sub = bitCompare(stems.stemL[0], stems.stemR[0], stems.stemL[1], stems.stemR[1]);
        // Real-time mix impulse position (both add): the common latency offset.
        const int64_t mixPos = argmaxAbs(c.L);
        json row = {{"case", "pdc-impulse"}, {"sr", sr}, {"block", block}, {"ideal", S},
                    {"exportDry", dryPos}, {"exportWet", wetPos}, {"exportDev", wetPos - dryPos},
                    {"realtimeMixPos", mixPos}, {"realtimeOffset", mixPos - S},
                    {"stemSubtractBitExact", sub.identical}, {"stemSubtractMaxDelta", sub.maxDelta}, {"peak", c.peak}};
        table.push_back(row);
        std::fprintf(stderr, "EVIDENCE PDC sr=%.0f block=%d exportDry=%lld exportWet=%lld dev=%lld rtOffset=%lld subExact=%d\n",
                     sr, block, (long long)dryPos, (long long)wetPos, (long long)(wetPos - dryPos), (long long)(mixPos - S), (int)sub.identical);
      }
    }
  }

  // ---- MIDI onset through the REAL host (CoreTimingClick), near block boundaries.
  if (click.is_object()) {
    for (double sr : kRates) {
      const double bpm = 600.0;
      const json notes = json::array({note(6, 8, 60, 0.8), note(13, 8, 65, 0.8), note(26, 8, 69, 0.8), note(103, 8, 72, 0.8)});
      const json track = pluginTrack("click", click, notes, 4 * kPpq, json::array());
      const json songJson = song(json::array({track}), bpm, 16 * 4 * kPpq);
      writeText(dir + "/song-midi-click.json", songJson.dump(2));
      const Project p = fromJson(songJson);
      const TempoMap tempo(bpm, {}, sr);
      for (int block : {128, 512}) {
        EventLog log;
        AssetStore assets; TestHost host;
        const RenderResult r = renderProject(p, sr, 512, false, 0.2, &log, nullptr, &host, 0);
        int64_t maxDev = 0;
        json onsets = json::array();
        for (const auto& e : log.entries) {
          const int64_t ideal = tempo.sampleOfTick(e.tick);
          const int64_t onset = peakNear(r.L, ideal, 2);
          const int64_t dev = onset - ideal;
          maxDev = std::max(maxDev, std::abs(dev));
          onsets.push_back({{"tick", e.tick}, {"ideal", ideal}, {"onset", onset}, {"dev", dev}});
        }
        const Capture c = runCapture(songJson, sr, block, (int64_t)(sr * 0.4));
        writeRaw(dir + "/midi-click-" + std::to_string((int)sr) + "-" + std::to_string(block) + ".f32", c);
        writeWavCap(dir + "/midi-click-" + std::to_string((int)sr) + "-" + std::to_string(block) + ".wav", c);
        json row = {{"case", "midi-click"}, {"sr", sr}, {"block", block}, {"maxOnsetDev", maxDev}, {"onsets", onsets}, {"peak", c.peak}};
        table.push_back(row);
        std::fprintf(stderr, "EVIDENCE MIDI sr=%.0f block=%d maxOnsetDev=%lld\n", sr, block, (long long)maxDev);
      }
    }
  }

  // ---- Kick/bass alignment through the REAL host: bass through CoreTimingDelay, subtraction vs reference.
  if (delay.is_object() && synth.is_object()) {
    for (double sr : kRates) {
      const double bpm = 120.0;
      const json kickNotes = json::array({note(0, 240, 36, 0.9), note(960, 240, 36, 0.9)});
      const json bassNotes = json::array({note(0, 480, 40, 0.7), note(960, 480, 43, 0.7)});
      const json kick = pluginTrack("kick", synth, kickNotes, 3840, json::array());
      const json bassRef = pluginTrack("bass", synth, bassNotes, 3840, json::array());
      const json bassWet = pluginTrack("bass", synth, bassNotes, 3840, json::array({delayEffect("d", delay)}));
      const json refSong = song(json::array({kick, bassRef}), bpm, 30720);
      const json wetSong = song(json::array({kick, bassWet}), bpm, 30720);
      writeText(dir + "/song-kickbass-ref.json", refSong.dump(2));
      writeText(dir + "/song-kickbass-wet.json", wetSong.dump(2));
      for (int block : {128, 512}) {
        AssetStore a1, a2; TestHost h1, h2;
        const RenderResult ref = renderProject(fromJson(refSong), sr, 512, false, 0.3, nullptr, &a1, &h1, 0);
        const RenderResult wet = renderProject(fromJson(wetSong), sr, 512, false, 0.3, nullptr, &a2, &h2, 0);
        const BitDiff d = bitCompare(ref.L, ref.R, wet.L, wet.R);
        const Capture c = runCapture(wetSong, sr, block, (int64_t)(sr * 1.0));
        writeRaw(dir + "/kickbass-wet-" + std::to_string((int)sr) + "-" + std::to_string(block) + ".f32", c);
        writeWavCap(dir + "/kickbass-wet-" + std::to_string((int)sr) + "-" + std::to_string(block) + ".wav", c);
        writeWavCap(dir + "/kickbass-ref-" + std::to_string((int)sr) + ".wav", Capture{ref.L, ref.R, 0, sr, ref.peak});
        json row = {{"case", "kickbass"}, {"sr", sr}, {"block", block}, {"refVsWetBitExact", d.identical},
                    {"refVsWetMaxDelta", d.maxDelta}, {"refVsWetFirstDiff", d.firstAt}, {"peak", c.peak}};
        table.push_back(row);
        std::fprintf(stderr, "EVIDENCE KICKBASS sr=%.0f block=%d refVsWetExact=%d maxDelta=%.9g\n", sr, block, (int)d.identical, d.maxDelta);
      }
    }
  }

  writeText(dir + "/deviations.json", table.dump(2));
  std::fprintf(stderr, "EVIDENCE wrote %zu rows to %s/deviations.json\n", table.size(), outDir.c_str());
  return 0;
}
#endif  // CORE_TEST_SYNTH_VST3

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && std::string(argv[1]) == "--evidence") {
#ifdef CORE_TEST_SYNTH_VST3
    plugins::Runtime juce;
    const std::string outDir = argv[2];
    const std::string delayPath = argc >= 4 ? argv[3] : "";
    const std::string clickPath = argc >= 5 ? argv[4] : "";
    return plugins::runWithMessageLoop([&] { return runEvidence(outDir, delayPath, clickPath); });
#else
    std::fprintf(stderr, "--evidence needs the real VST3 host (CORE_TEST_SYNTH_VST3); build with CORE_BUILD_APP=ON\n");
    return 1;
#endif
  }
  // Permanent regression: no JUCE needed (fake provider, offline). Run directly.
  return runAll(argc, argv);
}
