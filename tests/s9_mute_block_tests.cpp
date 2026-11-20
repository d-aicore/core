// S9 (owner round-2 «Restantes etapas», measurement-first): TWO SEPARATE problems, measured, not fixed.
// Contract: «Mede a CPU das faixas em mute e confirma o impacto do bloco de plugin fixo em 128. Antes de
// suspender processamento, cobre caudas, sidechain, latência e retoma sem clique. Mantém os dois problemas
// separados se exigirem correções distintas.»
//
// (A) MUTE CPU — CSV-09 confirmed: mute is consulted only at the FADER (graph.cpp:1329-1331 in the round-1
//     working tree), AFTER the instrument, the clips and every insert have run (:1266-1325). A muted track
//     pays full CPU. This suite MEASURES the waste per muted track (engine.cpu per-track/per-insert
//     attribution + the S7 seam's exact per-block timing), serial AND M5(4), null backend, Release.
//     DEFECT PIN: the cases below assert that a muted track's instrument/insert CPU is NON-ZERO — that is
//     the defect itself, pinned on purpose. When an authorized suspend-processing change lands, these pins
//     MUST be updated consciously with the new numbers (they are characterization, not a quality gate;
//     nothing here relaxes an existing criterion). The bypass contrast proves the gate's position: a
//     bypassed insert IS skipped (graph.cpp:1300) and costs ~0, a muted track's insert is not.
// (B) FIXED-128 BLOCK — juce_host.cpp:23 kPluginBlock=128 compile-time; live plugins ALWAYS gather into 128
//     (:410), the 256/512 knob is export-only (:26-40). The device block (64/128/256/512 here) does not
//     change the plugin block: it changes how many plugin calls fit in one callback and the per-callback
//     engine overhead. Measured: render time + load per device block, latency invariance (the gather adds
//     a fixed 128 samples to every plugin path, :162), plugin-call arithmetic. Separate verdict from (A).
// (C) THE FOUR COVERS the owner requires BEFORE suspending muted tracks:
//     C1 TAILS — mute today = immediate ramped silence (fader), the tail is still COMPUTED (CPU pin) but
//        never heard; a delay-line tail drains while muted because processing continues (probe mode with
//        the out-of-repo CoreTestDelay build: stale-tail-on-resume is what a suspend design must handle).
//     C2 SIDECHAIN — the M7c key path is captured PRE-fader/mute by design (graph.cpp:1328, «a muted ghost
//        kick still ducks», project.cpp:509-510) but DORMANT for plugins: Effect::sidechain is never
//        assigned (effects.hpp:54 default -1), PluginEffect::process passes no key (:247). Measured: the
//        sidechain JSON field changes nothing audible; routing validation is alive (negative controls).
//     C3 LATENCY — the PDC map is static per graph build (graph.cpp:610-672); mute/unmute rebuilds keep
//        latencySamples identical (instance + 128 gather). Measured invariance across mute states and
//        device blocks. A suspend design must not change reported latency (checkPluginLatency rebuilds,
//        server.cpp:288-292, and every delay line would shift).
//     C4 CLICK-FREE RESUME — today the plugin instance stays warm across the mute rebuild (shared_ptr,
//        adopt copies held pitches + ramp state, graph.cpp:190-211/856-857) and the fader ramps over an
//        aligned 32-sample sub-block. Measured with the EXISTING capture seam: ramped silence/resume, no
//        discontinuity above the signal's own slew, and BIT-EXACT steady-state resume vs an uninterrupted
//        reference (the plugin kept processing while muted — phase continuity). A suspend design must
//        preserve exactly this (warm state, ramp, no stale tail).
//
// Instrumentation REUSES the S7 seam (core::rtProbe, device.hpp:22-28): armed here with the per-block
// observer ONLY (exact renderNs/budgetNs per callback; no IAT hooks — violation detection is S7's suite).
// CPU attribution = engine.cpu (CpuMeter relaxed atomics, M5-worker safe, graph.hpp:45-55). Capture =
// Engine::startCapture (copies AFTER the live-worker mix, BEFORE the ear guard — the m5_bitexact seam).
// NO engine source change; the only repo edits are this file and its additive registration block.
//
// Modes:
//   s9_mute_block_tests [name-filter]    permanent characterization suite (ctest -L merge).
//   s9_mute_block_tests --measure <outDir> [seconds]
//                                        full A/B matrix as JSON evidence (default 3 s windows).
//   s9_mute_block_tests --covers <outDir> [delayPluginPath]
//                                        C1-C4 capture evidence; with the out-of-repo CoreTestDelay VST3
//                                        (built from app/selftest-plugins/TestPlugins.cpp, CORE_TEST_DELAY,
//                                        no repo target) the 480-sample tail + 608-sample latency runs.
// M5 stays OFF by default (D51): parallel runs opt in by argument; LiveEnvGuard clears CORE_LIVE_THREADS.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "support/check.hpp"
#include "core/engine.hpp"
#include "core/project.hpp"
#include "core/server.hpp"
#include "io/device.hpp"
#include "plugins/juce_host.hpp"

using namespace core;
using namespace coretest;
using namespace std::chrono_literals;

namespace {

const double kSr = 48000;

// ---------------------------------------------------------------- S7 seam reuse: per-block timing observer
struct BlockRec { uint64_t renderNs, budgetNs, gapNs, frames; };
constexpr uint64_t kBlockCap = 65536;  // one 3 s scenario at 64/48000 = 2250 blocks; the matrix fits whole
BlockRec g_blocks[kBlockCap];
std::atomic<uint64_t> g_blockCount{0};
std::atomic<uint64_t> g_blockOverflow{0};
void probeBlock(uint64_t renderNs, uint64_t budgetNs, uint64_t gapNs, uint64_t frames) {
  const uint64_t i = g_blockCount.fetch_add(1, std::memory_order_relaxed);
  if (i < kBlockCap) g_blocks[i] = {renderNs, budgetNs, gapNs, frames};
  else g_blockOverflow.fetch_add(1, std::memory_order_relaxed);
}
std::atomic<bool> g_inCallback{false};
std::atomic<uint32_t> g_audioTid{0};
RtProbe g_probeState{&g_inCallback, &g_audioTid, nullptr, probeBlock};
void armTiming() { rtProbe.store(&g_probeState, std::memory_order_release); }
void disarmTiming() { rtProbe.store(nullptr, std::memory_order_release); g_inCallback.store(false, std::memory_order_relaxed); }

struct TimingStats {
  uint64_t blocks = 0, framesTotal = 0, overBudget = 0;
  double maxMs = 0, p99Ms = 0, p95Ms = 0, p50Ms = 0, meanMs = 0;
  double meanLoadPct = 0, p99LoadPct = 0, maxLoadPct = 0;
};
TimingStats timingFromRing(uint64_t from, uint64_t to) {
  TimingStats t;
  const uint64_t n = std::min<uint64_t>(to, kBlockCap);
  if (from >= n) return t;
  std::vector<uint64_t> ns;
  ns.reserve((size_t)(n - from));
  double loadSum = 0;
  for (uint64_t i = from; i < n; ++i) {
    ns.push_back(g_blocks[i].renderNs);
    t.framesTotal += g_blocks[i].frames;
    if (g_blocks[i].renderNs > g_blocks[i].budgetNs) ++t.overBudget;
    if (g_blocks[i].budgetNs) loadSum += (double)g_blocks[i].renderNs * 100.0 / (double)g_blocks[i].budgetNs;
  }
  std::sort(ns.begin(), ns.end());
  t.blocks = ns.size();
  auto pct = [&](double p) { return (double)ns[std::min<size_t>(ns.size() - 1, (size_t)(p / 100.0 * (double)ns.size()))]; };
  t.maxMs = (double)ns.back() / 1e6;
  t.p99Ms = pct(99) / 1e6; t.p95Ms = pct(95) / 1e6; t.p50Ms = pct(50) / 1e6;
  uint64_t sum = 0; for (auto v : ns) sum += v;
  t.meanMs = (double)sum / (double)ns.size() / 1e6;
  t.meanLoadPct = loadSum / (double)ns.size();
  std::vector<double> loads;
  loads.reserve(ns.size());
  for (uint64_t i = from; i < n; ++i)
    if (g_blocks[i].budgetNs) loads.push_back((double)g_blocks[i].renderNs * 100.0 / (double)g_blocks[i].budgetNs);
  if (!loads.empty()) {
    std::sort(loads.begin(), loads.end());
    t.maxLoadPct = loads.back();
    t.p99LoadPct = loads[std::min<size_t>(loads.size() - 1, (size_t)(0.99 * (double)loads.size()))];
  }
  return t;
}

// ---------------------------------------------------------------- harness (the serve architecture, as s7/m5)
struct LiveEnvGuard {
  std::string saved; bool had = false;
  LiveEnvGuard() {
    char* buf = nullptr; size_t len = 0;
    if (_dupenv_s(&buf, &len, "CORE_LIVE_THREADS") == 0 && buf) { had = true; saved = buf; free(buf); }
    set("");
  }
  void set(const char* v) { _putenv_s("CORE_LIVE_THREADS", v); }
  ~LiveEnvGuard() { set(had ? saved.c_str() : ""); }
};
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
json note(int64_t tick, int64_t length, int pitch, double velocity) {
  return {{"tick", tick}, {"length", length}, {"pitch", pitch}, {"velocity", velocity}};
}
// The reference song (m5_bitexact/s7): kick + bass + lead through CoreTestSynth, bass through CoreTestGain.
json referenceSong(const json& synth, const json& gain) {
  const json kick = {{"id", "kick"}, {"name", "Kick"}, {"instrument", "plugin"}, {"plugin", synth},
                     {"patternTicks", 3840},
                     {"notes", json::array({note(0, 240, 36, 0.9), note(960, 240, 36, 0.9)})}};
  const json bass = {{"id", "bass"}, {"name", "Bass"}, {"instrument", "plugin"}, {"plugin", synth},
                     {"patternTicks", 3840},
                     {"notes", json::array({note(0, 480, 40, 0.7), note(960, 480, 43, 0.7)})},
                     {"effects", json::array({{{"type", "plugin"}, {"id", "gainfx"}, {"plugin", gain}}})}};
  const json lead = {{"id", "lead"}, {"name", "Lead"}, {"instrument", "plugin"}, {"plugin", synth},
                     {"patternTicks", 3840},
                     {"notes", json::array({note(480, 240, 64, 0.5), note(1440, 240, 67, 0.5)})}};
  return {{"format", kProjectFormat}, {"version", 2}, {"name", "s9 mute/block reference"}, {"bpm", 120.0},
          {"lengthTicks", 30720}, {"masterGainDb", 0.0}, {"tracks", json::array({kick, bass, lead})}};
}
json withMutes(json song, const std::vector<std::string>& ids) {
  for (auto& t : song["tracks"])
    if (std::find(ids.begin(), ids.end(), t.value("id", "")) != ids.end()) t["mute"] = true;
  return song;
}
json withBypassFx(json song) {
  for (auto& t : song["tracks"])
    if (t.value("id", "") == "bass") t["effects"][0]["bypass"] = true;
  return song;
}
// C2: the bass's gain insert carries (or not) the M7c sidechain field naming the kick. The JSON passes it
// through to the graph (project.cpp:474-480); validateRouting checks it (:128-140); no Effect consumes it.
json sidechainSong(const json& synth, const json& gain, bool withField, bool kickMuted) {
  json song = referenceSong(synth, gain);
  song["name"] = "s9 sidechain dormancy";
  if (withField) song["tracks"][1]["effects"][0]["sidechain"] = "kick";
  if (kickMuted) song["tracks"][0]["mute"] = true;
  return song;
}
// C1/C4: one track, one continuous note for the whole 3 s capture (pattern == note length: no wrap).
json longNoteSong(const json& synth) {
  const json lead = {{"id", "lead"}, {"name", "Lead"}, {"instrument", "plugin"}, {"plugin", synth},
                     {"patternTicks", 5760},
                     {"notes", json::array({note(0, 5760, 64, 0.5)})}};
  return {{"format", kProjectFormat}, {"version", 2}, {"name", "s9 long note"}, {"bpm", 120.0},
          {"lengthTicks", 30720}, {"masterGainDb", 0.0}, {"tracks", json::array({lead})}};
}
// C1 probe: a 0.5 s note through the 480-sample delay line (out-of-repo CoreTestDelay build). The tail
// (the line's stored signal) leaves the plugin 480+128 samples after the note's last sample.
json delayTailSong(const json& synth, const json& delay) {
  const json src = {{"id", "src"}, {"name", "Src"}, {"instrument", "plugin"}, {"plugin", synth},
                    {"patternTicks", 3840},
                    {"notes", json::array({note(0, 960, 60, 0.8)})},
                    {"effects", json::array({{{"type", "plugin"}, {"id", "delayfx"}, {"plugin", delay}}})}};
  return {{"format", kProjectFormat}, {"version", 2}, {"name", "s9 delay tail"}, {"bpm", 120.0},
          {"lengthTicks", 30720}, {"masterGainDb", 0.0}, {"tracks", json::array({src})}};
}

// ---------------------------------------------------------------- measured run (S7 window discipline)
struct RunResult {
  json device, stats, cpu;
  TimingStats timing;
  uint64_t workerJobs = 0;
  double deliveredSeconds = 0, wallSeconds = 0;
  int liveThreads = 0;
  std::string error;
};
RunResult measureRun(const json& song, int liveThreads, int block, double seconds) {
  RunResult r;
  r.liveThreads = liveThreads;
  const auto t0 = std::chrono::steady_clock::now();
  TestHost host;
  Server server(makeMiniaudioOutput, kSr, &host);
  const json loaded = server.execute("project.load", {{"project", song}});
  if (!loaded.value("missing", json::array()).empty()) { r.error = "plugins missing: " + loaded["missing"].dump(); return r; }
  json start;
  try { start = server.execute("engine.start", {{"backend", "null"}, {"blockSize", block}, {"liveThreads", liveThreads}}); }
  catch (const std::exception& e) { r.error = std::string("engine.start: ") + e.what(); return r; }
  r.device = start;
  Engine& e = server.engine();
  armTiming();
  std::this_thread::sleep_for(2s);  // warmup: plugins settle, device runs silence
  server.execute("engine.resetStats", json::object());
  const uint64_t blockFrom = g_blockCount.load(std::memory_order_relaxed);
  const uint64_t jobs0 = e.livePool() ? e.livePool()->workerJobs() : 0;
  server.execute("engine.cpu", json::object());  // interval start
  server.execute("transport.play", json::object());
  const auto playStart = std::chrono::steady_clock::now();
  const auto hardDeadline = playStart + std::chrono::duration<double>(seconds + 10.0);
  while (std::chrono::steady_clock::now() - playStart < std::chrono::duration<double>(seconds) &&
         std::chrono::steady_clock::now() < hardDeadline)
    std::this_thread::sleep_for(20ms);  // ONLY sleeps inside the window
  const uint64_t blockTo = g_blockCount.load(std::memory_order_relaxed);
  const uint64_t jobs1 = e.livePool() ? e.livePool()->workerJobs() : 0;
  r.stats = server.execute("engine.stats", json::object());  // BEFORE stop: stop rebuilds (zeroed meters)
  r.cpu = server.execute("engine.cpu", json::object());
  server.execute("transport.stop", json::object());
  server.execute("engine.stop", json::object());
  disarmTiming();
  r.workerJobs = jobs1 - jobs0;
  r.timing = timingFromRing(blockFrom, blockTo);
  r.deliveredSeconds = (double)r.timing.framesTotal / kSr;
  r.wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return r;
}
void printRun(const char* label, const RunResult& r) {
  std::fprintf(stderr, "S9 %s: threads=%d block=%d callbacks=%llu delivered=%.2fs meanMs=%.4f p50=%.4f p99=%.4f maxMs=%.4f load mean=%.2f%% p99=%.2f%% overBudget=%llu\n",
               label, r.liveThreads, r.device.value("blockSize", 0),
               (unsigned long long)r.stats.value("callbacks", (uint64_t)0), r.deliveredSeconds,
               r.timing.meanMs, r.timing.p50Ms, r.timing.p99Ms, r.timing.maxMs,
               r.timing.meanLoadPct, r.timing.p99LoadPct, (unsigned long long)r.timing.overBudget);
  std::fprintf(stderr, "S9 %s: cpu totalPct=%.3f intervalMs=%.0f workerJobs=%llu latencyTracks=%d latencyMaster=%d\n",
               label, r.cpu.value("totalPct", 0.0), r.cpu.value("intervalMs", 0.0),
               (unsigned long long)r.workerJobs,
               r.stats.value("latency", json::object()).value("tracksSamples", -1),
               r.stats.value("latency", json::object()).value("masterSamples", -1));
  for (const auto& t : r.cpu.value("tracks", json::array())) {
    std::fprintf(stderr, "S9 %s:   track %-5s cpuPct=%.3f instrument=%.3f", label,
                 t.value("id", "?").c_str(), t.value("cpuPct", 0.0),
                 t.value("instrument", json::object()).value("cpuPct", 0.0));
    for (const auto& fx : t.value("effects", json::array()))
      std::fprintf(stderr, " fx:%s=%.3f", fx.value("id", "?").c_str(), fx.value("cpuPct", 0.0));
    std::fprintf(stderr, "\n");
  }
  if (!r.error.empty()) std::fprintf(stderr, "S9 %s: ERROR %s\n", label, r.error.c_str());
}
void characterizeRun(const char* label, RunResult& r) {
  printRun(label, r);
  CHECK(r.error.empty());
  CHECK(r.stats.value("callbacks", (uint64_t)0) > (uint64_t)0);
  CHECK(r.deliveredSeconds > 1.0);
  CHECK(r.timing.blocks > 100);
  CHECK_NEAR(r.timing.maxMs, r.stats.value("maxRenderMs", 0.0), 0.001);  // seam and engine agree
  CHECK(r.cpu.value("totalPct", 0.0) > 0.0);  // the plugins really worked
}
double trackCpu(const json& cpu, const std::string& id, bool instrumentOnly) {
  for (const auto& t : cpu.value("tracks", json::array()))
    if (t.value("id", "") == id)
      return instrumentOnly ? t.value("instrument", json::object()).value("cpuPct", 0.0) : t.value("cpuPct", 0.0);
  return -1;
}
double fxCpu(const json& cpu, const std::string& trackId, const std::string& fxId) {
  for (const auto& t : cpu.value("tracks", json::array()))
    if (t.value("id", "") == trackId)
      for (const auto& fx : t.value("effects", json::array()))
        if (fx.value("id", "") == fxId) return fx.value("cpuPct", 0.0);
  return -1;
}
json timingJson(const TimingStats& t) {
  return {{"blocks", t.blocks}, {"framesTotal", t.framesTotal}, {"maxMs", t.maxMs}, {"p99Ms", t.p99Ms},
          {"p95Ms", t.p95Ms}, {"p50Ms", t.p50Ms}, {"meanMs", t.meanMs}, {"overBudget", t.overBudget},
          {"meanLoadPct", t.meanLoadPct}, {"p99LoadPct", t.p99LoadPct}, {"maxLoadPct", t.maxLoadPct}};
}
json runJson(const char* scenario, const RunResult& r) {
  return {{"scenario", scenario}, {"liveThreads", r.liveThreads}, {"device", r.device}, {"stats", r.stats},
          {"cpu", r.cpu}, {"timing", timingJson(r.timing)}, {"workerJobs", r.workerJobs},
          {"deliveredSeconds", r.deliveredSeconds}, {"wallSeconds", r.wallSeconds}, {"error", r.error}};
}
void writeJson(const std::string& path, const json& j) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (f) { std::fputs(j.dump(2).c_str(), f); std::fclose(f); }
}

// ---------------------------------------------------------------- capture runs (the m5 seam) + analysis
struct CaptureResult {
  std::vector<float> L, R;
  std::vector<int64_t> swaps;
  int64_t frames = 0;
  uint64_t callbacks = 0;
  json latency;
  float peak = 0;
  std::string error;
};
struct Action { int64_t atSample; std::string trackId; bool mute; };
CaptureResult runCapture(const json& song, int liveThreads, int block, int64_t frames, const std::vector<Action>& actions) {
  CaptureResult r;
  TestHost host;
  Server server(makeMiniaudioOutput, kSr, &host);
  const json loaded = server.execute("project.load", {{"project", song}});
  if (!loaded.value("missing", json::array()).empty()) { r.error = "plugins missing: " + loaded["missing"].dump(); return r; }
  json start;
  try { start = server.execute("engine.start", {{"backend", "null"}, {"blockSize", block}, {"liveThreads", liveThreads}}); }
  catch (const std::exception& e) { r.error = std::string("engine.start: ") + e.what(); return r; }
  Engine& e = server.engine();
  e.startCapture(frames);
  server.execute("transport.play", json::object());
  size_t next = 0;
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (e.captured() < frames && std::chrono::steady_clock::now() < deadline) {
    while (next < actions.size() && e.position() >= actions[next].atSample) {
      server.execute("track.set", {{"trackId", actions[next].trackId}, {"patch", {{"mute", actions[next].mute}}}});
      ++next;
    }
    std::this_thread::sleep_for(2ms);
  }
  while (next < actions.size()) {  // the capture ended before an action's sample: apply it anyway (recorded)
    server.execute("track.set", {{"trackId", actions[next].trackId}, {"patch", {{"mute", actions[next].mute}}}});
    ++next;
  }
  e.stopCapture();
  r.frames = e.captured();
  const json stats = server.execute("engine.stats", json::object());
  r.callbacks = stats.value("callbacks", (uint64_t)0);
  r.latency = stats.value("latency", json::object());
  r.swaps = e.captureSwaps();
  r.L.assign(e.captureL().begin(), e.captureL().begin() + (size_t)r.frames);
  r.R.assign(e.captureR().begin(), e.captureR().begin() + (size_t)r.frames);
  for (size_t i = 0; i < r.L.size(); ++i) r.peak = std::max({r.peak, std::fabs(r.L[i]), std::fabs(r.R[i])});
  server.execute("transport.stop", json::object());
  server.execute("engine.stop", json::object());
  if (r.frames < frames) r.error = "capture short: " + std::to_string(r.frames) + " < " + std::to_string(frames);
  return r;
}
float maxSlew(const std::vector<float>& x, int64_t from, int64_t to) {
  from = std::max<int64_t>(from, 1); to = std::min<int64_t>(to, (int64_t)x.size());
  float m = 0;
  for (int64_t i = from; i < to; ++i) m = std::max(m, std::fabs(x[i] - x[i - 1]));
  return m;
}
double rms(const std::vector<float>& x, int64_t from, int64_t to) {
  from = std::max<int64_t>(from, 0); to = std::min<int64_t>(to, (int64_t)x.size());
  if (to <= from) return 0;
  double s = 0;
  for (int64_t i = from; i < to; ++i) s += (double)x[i] * x[i];
  return std::sqrt(s / (double)(to - from));
}
bool bitExactTail(const CaptureResult& a, const CaptureResult& b, int64_t tailFrames) {
  if (a.frames < tailFrames || b.frames < tailFrames) return false;
  const size_t offA = (size_t)(a.frames - tailFrames), offB = (size_t)(b.frames - tailFrames);
  return std::memcmp(a.L.data() + offA, b.L.data() + offB, (size_t)tailFrames * 4) == 0 &&
         std::memcmp(a.R.data() + offA, b.R.data() + offB, (size_t)tailFrames * 4) == 0;
}
int64_t firstSwapAtOrAfter(const CaptureResult& r, int64_t sample) {
  for (int64_t s : r.swaps) if (s >= sample) return s;
  return -1;
}
// First index >= from where the next hold samples are all (near-)zero; -1 if none within span.
int64_t firstSilence(const std::vector<float>& x, int64_t from, int64_t span, int64_t hold) {
  const int64_t end = std::min<int64_t>((int64_t)x.size() - hold, from + span);
  for (int64_t i = from; i < end; ++i) {
    bool silent = true;
    for (int64_t j = i; j < i + hold; ++j) if (std::fabs(x[j]) > 1e-7f) { silent = false; break; }
    if (silent) return i;
  }
  return -1;
}
int64_t firstSignal(const std::vector<float>& x, int64_t from, int64_t span) {
  const int64_t end = std::min<int64_t>((int64_t)x.size(), from + span);
  for (int64_t i = from; i < end; ++i) if (std::fabs(x[i]) > 1e-4f) return i;
  return -1;
}
void writeCaptureRaw(const std::string& path, const CaptureResult& r) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fwrite(r.L.data(), 4, r.L.size(), f);
  std::fwrite(r.R.data(), 4, r.R.size(), f);
  std::fclose(f);
}
// The C1/C4 transition analysis shared by the gate case and --covers.
json analyzeMuteResume(const CaptureResult& ref, const CaptureResult& mut, int64_t muteAt, int64_t unmuteAt) {
  json j;
  const int64_t s0 = firstSwapAtOrAfter(mut, muteAt);
  const int64_t s1 = firstSwapAtOrAfter(mut, unmuteAt);
  j["muteSwapAt"] = s0; j["unmuteSwapAt"] = s1;
  j["swaps"] = mut.swaps;
  j["peakReference"] = ref.peak; j["peakMuteRun"] = mut.peak;
  const float steady = maxSlew(ref.L, 9600, 19200);  // 0.2-0.4 s: the signal's own max step
  j["maxSlewSteady"] = steady;
  if (s0 >= 0) {
    const int64_t z = firstSilence(mut.L, s0, 4096, 512);
    j["rampToSilenceSamples"] = z >= 0 ? z - s0 : -1;
    j["maxSlewMuteWindow"] = maxSlew(mut.L, s0 - 64, s0 + 512);
    j["rmsMutedWindow"] = rms(mut.L, s0 + 512, std::min<int64_t>(s0 + 4800, unmuteAt - 512));
  }
  if (s1 >= 0) {
    const int64_t z = firstSignal(mut.L, s1, 4096);
    j["rampToSignalSamples"] = z >= 0 ? z - s1 : -1;
    j["maxSlewUnmuteWindow"] = maxSlew(mut.L, s1 - 64, s1 + 512);
    j["peakAfterResume"] = 0.0f;
    for (int64_t i = s1 + 512; i < mut.frames; ++i) j["peakAfterResume"] = std::max(j["peakAfterResume"].get<float>(), std::fabs(mut.L[i]));
  }
  j["steadyTailBitExact"] = bitExactTail(ref, mut, 24000);  // last 0.5 s: warm state, fader 1 in both
  return j;
}

}  // namespace

// ================================================================ permanent characterization suite

// (A) DEFECT PIN (CSV-09): a muted track's instrument AND inserts are fully processed. The numbers are the
// waste; the pins (>0) must be consciously updated when an authorized suspend change lands.
TEST("S9 A mute CPU: muted tracks pay full processing (CSV-09 defect pin), serial + M5(4)") {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  CHECK(synth.is_object()); CHECK(gain.is_object());
  if (!synth.is_object() || !gain.is_object()) return;
  const json song = referenceSong(synth, gain);
  const double blockMs = 128.0 / kSr * 1000.0;
  for (int threads : {0, 4}) {
    RunResult none = measureRun(song, threads, 128, 2.0);
    characterizeRun(threads ? "a-none-m5" : "a-none-serial", none);
    RunResult one = measureRun(withMutes(song, {"kick"}), threads, 128, 2.0);
    characterizeRun(threads ? "a-mute1-m5" : "a-mute1-serial", one);
    RunResult all = measureRun(withMutes(song, {"kick", "bass", "lead"}), threads, 128, 2.0);
    characterizeRun(threads ? "a-mute3-m5" : "a-mute3-serial", all);
    // The defect, quantified: muted tracks cost what unmuted tracks cost (per-track CPU, % of one core).
    for (const char* id : {"kick", "bass", "lead"}) {
      const double unmuted = trackCpu(none.cpu, id, false);
      const double muted = trackCpu(all.cpu, id, false);
      CHECK(unmuted > 0.0);
      CHECK(muted > 0.0);  // DEFECT PIN: mute does not suspend processing (graph.cpp:1329 fader-only gate)
      std::fprintf(stderr, "S9 A t%d track %s: unmuted=%.3f%% muted=%.3f%% of one core; wasted=%.4f ms/block (%.1f%% of the 128-block budget)\n",
                   threads, id, unmuted, muted, muted / 100.0 * blockMs, muted / 100.0 * blockMs / blockMs * 100.0);
    }
    CHECK(trackCpu(all.cpu, "bass", true) > 0.0);   // instrument runs while muted
    CHECK(fxCpu(all.cpu, "bass", "gainfx") > 0.0);  // insert runs while muted
    // Whole-block render time: muting everything saves ~nothing (the defect at the callback level).
    std::fprintf(stderr, "S9 A t%d: meanMs none=%.4f mute1=%.4f mute3=%.4f (ratio mute3/none=%.3f); cpu totalPct none=%.3f mute3=%.3f\n",
                 threads, none.timing.meanMs, one.timing.meanMs, all.timing.meanMs,
                 all.timing.meanMs / std::max(1e-9, none.timing.meanMs),
                 none.cpu.value("totalPct", 0.0), all.cpu.value("totalPct", 0.0));
    if (threads == 4) {
      CHECK(none.workerJobs > (uint64_t)0);
      CHECK(!none.stats.value("liveFallback", false));
      CHECK_EQ(none.stats.value("liveStalls", (uint64_t)0), (uint64_t)0);
    }
  }
  // The contrast that locates the gate: BYPASS skips the insert's processing (graph.cpp:1300), mute does not.
  RunResult bypass = measureRun(withBypassFx(song), 0, 128, 2.0);
  characterizeRun("a-bypassfx-serial", bypass);
  CHECK(fxCpu(bypass.cpu, "bass", "gainfx") >= 0.0);
  CHECK(fxCpu(bypass.cpu, "bass", "gainfx") < 1e-6);  // bypassed: no CPU at all
  CHECK(trackCpu(bypass.cpu, "bass", true) > 0.0);    // the instrument still runs (bypass is per-insert)
}

// (B) The live plugin block is compile-time 128 regardless of the device block: latency is constant, the
// plugin-call count per callback is block/128, and the gather copies every sample at every device block.
TEST("S9 B fixed-128 block: device 64/128/256/512, latency constant, serial + M5(4)") {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  CHECK(synth.is_object()); CHECK(gain.is_object());
  if (!synth.is_object() || !gain.is_object()) return;
  const json song = referenceSong(synth, gain);
  int latencyRef = -1;
  for (int threads : {0, 4}) {
    for (int block : {64, 128, 256, 512}) {
      RunResult r = measureRun(song, threads, block, 2.0);
      char label[64]; std::snprintf(label, sizeof label, "b-t%d-%d", threads, block);
      characterizeRun(label, r);
      CHECK_EQ(r.device.value("blockSize", 0), block);  // the null backend honours the request
      const int lat = r.stats.value("latency", json::object()).value("tracksSamples", -1);
      if (latencyRef < 0) latencyRef = lat;
      CHECK_EQ(lat, latencyRef);  // the gather's 128 samples do not depend on the device block
      // 4 plugin instances (3 synth + 1 gain); each gathers into 128-sample calls (juce_host.cpp:142-159).
      const double pluginCalls = (double)r.timing.framesTotal / 128.0 * 4.0;
      std::fprintf(stderr, "S9 B %s: pluginCallsPerCallback=%.2f expectedPluginCalls=%.0f gatherLatencyPerPath=128 latencyTracks=%d\n",
                   label, block / 128.0, pluginCalls, lat);
      if (threads == 4) {
        CHECK(r.workerJobs > (uint64_t)0);
        CHECK(!r.stats.value("liveFallback", false));
        CHECK_EQ(r.stats.value("liveStalls", (uint64_t)0), (uint64_t)0);
      }
    }
  }
  // Reference song PDC: bass = synth gather 128 + gain gather 128 = 256; the others wait for it.
  CHECK_EQ(latencyRef, 256);
}

// (C1+C4) Mute mid-play: immediate ramped silence (the tail is computed, never heard); unmute: ramped,
// click-free, and the steady state is BIT-EXACT with an uninterrupted reference — the instance stayed warm.
TEST("S9 C1+C4 mute/unmute mid-play: ramped silence, click-free resume, warm plugin state (capture)") {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  CHECK(synth.is_object());
  if (!synth.is_object()) return;
  const json song = longNoteSong(synth);
  const int64_t frames = 144000;  // 3 s
  CaptureResult ref = runCapture(song, 0, 128, frames, {});
  CaptureResult mut = runCapture(song, 0, 128, frames, {{48000, "lead", true}, {96000, "lead", false}});
  CHECK(ref.error.empty()); CHECK(mut.error.empty());
  CHECK(ref.peak > 0.05f); CHECK(mut.peak > 0.05f);
  const json j = analyzeMuteResume(ref, mut, 48000, 96000);
  std::fprintf(stderr, "S9 C1+C4: %s\n", j.dump().c_str());
  const int64_t s0 = j.value("muteSwapAt", (int64_t)-1), s1 = j.value("unmuteSwapAt", (int64_t)-1);
  CHECK(s0 >= 0); CHECK(s1 >= 0);
  // Mute semantics today: silence within a few aligned sub-blocks (32-sample ramp + swap alignment).
  CHECK(j.value("rampToSilenceSamples", (int64_t)-1) >= 0);
  CHECK(j.value("rampToSilenceSamples", (int64_t)999) <= 256);
  CHECK(j.value("rmsMutedWindow", 1.0) < 1e-6);  // fully silent while muted (tail computed, not heard)
  // Click-free: no step in the transition windows beyond 4x the signal's own max slew (a hard cut of this
  // sine would be ~20x). The ramp is the existing fader ramp, adopted across the rebuild.
  const float steady = j.value("maxSlewSteady", 0.0f);
  CHECK(steady > 0.0f);
  CHECK(j.value("maxSlewMuteWindow", 1.0f) <= 4.0f * steady + 1e-5f);
  CHECK(j.value("maxSlewUnmuteWindow", 1.0f) <= 4.0f * steady + 1e-5f);
  // Resume: the signal returns, ramped, and the steady state matches the uninterrupted reference bit for
  // bit — the plugin kept processing while muted (phase continuity). A suspend design must preserve this.
  CHECK(j.value("rampToSignalSamples", (int64_t)-1) >= 0);
  CHECK(j.value("rampToSignalSamples", (int64_t)999) <= 256);
  CHECK(j.value("peakAfterResume", 0.0f) > 0.05f);
  CHECK(j.value("steadyTailBitExact", false));
  // M5(4): the same warm resume through the live workers.
  CaptureResult mutM5 = runCapture(song, 4, 128, frames, {{48000, "lead", true}, {96000, "lead", false}});
  CHECK(mutM5.error.empty());
  const json j5 = analyzeMuteResume(ref, mutM5, 48000, 96000);
  std::fprintf(stderr, "S9 C1+C4 m5: %s\n", j5.dump().c_str());
  CHECK(j5.value("steadyTailBitExact", false));
  CHECK(j5.value("maxSlewMuteWindow", 1.0f) <= 4.0f * steady + 1e-5f);
  CHECK(j5.value("maxSlewUnmuteWindow", 1.0f) <= 4.0f * steady + 1e-5f);
}

// (C2) The M7c sidechain path is DORMANT for plugin effects: the JSON field is accepted and validated, but
// no Effect consumes the key (Effect::sidechain never assigned; PluginEffect passes no key). Muting the
// key source therefore cannot break any sidechain today — a suspend design must still honour the PRE-mute
// key contract («a muted ghost kick still ducks», project.cpp:509-510) for when plugin sidechains land.
TEST("S9 C2 sidechain: key path dormant for plugins, routing validation alive (capture + negative controls)") {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  CHECK(synth.is_object()); CHECK(gain.is_object());
  if (!synth.is_object() || !gain.is_object()) return;
  const int64_t frames = 48000;  // 1 s
  // With the field vs without, the kick (the key source) muted in both: the bass must sound IDENTICAL.
  CaptureResult with = runCapture(sidechainSong(synth, gain, true, true), 0, 128, frames, {});
  CaptureResult without = runCapture(sidechainSong(synth, gain, false, true), 0, 128, frames, {});
  CHECK(with.error.empty()); CHECK(without.error.empty());
  CHECK(with.peak > 0.05f);  // the bass really sounds (the kick is muted)
  const bool identical = with.frames == without.frames &&
      std::memcmp(with.L.data(), without.L.data(), (size_t)with.frames * 4) == 0 &&
      std::memcmp(with.R.data(), without.R.data(), (size_t)with.frames * 4) == 0;
  std::fprintf(stderr, "S9 C2: sidechain-field capture bit-identical to no-field (kick muted both): %d\n", (int)identical);
  CHECK(identical);  // dormancy: the key changes nothing audible
  CHECK_EQ(with.latency.value("tracksSamples", -1), without.latency.value("tracksSamples", -2));
  // The routing validation is alive (the field is checked even though no effect consumes it).
  {
    TestHost host;
    Server server(makeMiniaudioOutput, kSr, &host);
    json self = sidechainSong(synth, gain, true, false);
    self["tracks"][1]["effects"][0]["sidechain"] = "bass";  // from itself
    CHECK_THROWS(server.execute("project.load", {{"project", self}}));
    json busSong = referenceSong(synth, gain);
    busSong["tracks"].push_back({{"id", "bus1"}, {"name", "Bus"}, {"instrument", "bus"}});
    busSong["tracks"][1]["effects"][0]["sidechain"] = "bus1";  // from a bus
    CHECK_THROWS(server.execute("project.load", {{"project", busSong}}));
    json missing = sidechainSong(synth, gain, true, false);
    missing["tracks"][1]["effects"][0]["sidechain"] = "nope";  // from a track that does not exist
    CHECK_THROWS(server.execute("project.load", {{"project", missing}}));
  }
}

// (C3) The PDC map is invariant under mute/unmute: every rebuild reports the same latency (the gather's
// 128 samples per plugin path are constant; suspend must keep it that way or every delay line shifts).
TEST("S9 C3 latency: PDC map invariant under mute/unmute rebuilds") {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  CHECK(synth.is_object()); CHECK(gain.is_object());
  if (!synth.is_object() || !gain.is_object()) return;
  TestHost host;
  Server server(makeMiniaudioOutput, kSr, &host);
  const json loaded = server.execute("project.load", {{"project", referenceSong(synth, gain)}});
  CHECK(loaded.value("missing", json::array()).empty());
  server.execute("engine.start", {{"backend", "null"}, {"blockSize", 128}, {"liveThreads", 0}});
  auto lat = [&] { return server.execute("engine.stats", json::object()).value("latency", json::object()); };
  const json l0 = lat();
  std::fprintf(stderr, "S9 C3: latency unmuted: %s\n", l0.dump().c_str());
  CHECK_EQ(l0.value("tracksSamples", -1), 256);  // bass: synth gather 128 + gain gather 128
  CHECK_EQ(l0.value("masterSamples", -1), 0);
  int step = 0;
  for (const char* id : {"kick", "bass", "lead"}) {
    server.execute("track.set", {{"trackId", id}, {"patch", {{"mute", true}}}});
    const json l = lat();
    std::fprintf(stderr, "S9 C3: latency after mute %s: %s\n", id, l.dump().c_str());
    CHECK_EQ(l.value("tracksSamples", -1), l0.value("tracksSamples", -2));
    CHECK_EQ(l.value("masterSamples", -1), l0.value("masterSamples", -2));
    ++step;
  }
  for (const char* id : {"kick", "bass", "lead"}) {
    server.execute("track.set", {{"trackId", id}, {"patch", {{"mute", false}}}});
    const json l = lat();
    CHECK_EQ(l.value("tracksSamples", -1), l0.value("tracksSamples", -2));
    CHECK_EQ(l.value("masterSamples", -1), l0.value("masterSamples", -2));
  }
  CHECK_EQ(step, 3);
  server.execute("engine.stop", json::object());
}

// ================================================================ --measure: the full A/B matrix as evidence

namespace {

int runMeasure(const char* outDir, double seconds) {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  if (!synth.is_object() || !gain.is_object()) { std::fprintf(stderr, "S9 measure: test plugins not found\n"); return 1; }
  const json song = referenceSong(synth, gain);
  const json plugins = json::array({
    {{"role", "instrument"}, {"name", synth.value("name", "?")}, {"file", synth.value("file", "?")}},
    {{"role", "insert"}, {"name", gain.value("name", "?")}, {"file", gain.value("file", "?")}}});
  json summary = json::array();
  struct Scenario { const char* name; std::vector<std::string> mutes; bool bypass; };
  const Scenario a[] = {{"a-none", {}, false}, {"a-mute1", {"kick"}, false}, {"a-mute2", {"kick", "bass"}, false},
                        {"a-mute3", {"kick", "bass", "lead"}, false}, {"a-bypassfx", {}, true}};
  for (int threads : {0, 4}) {
    for (const auto& sc : a) {
      json s = sc.bypass ? withBypassFx(song) : withMutes(song, sc.mutes);
      RunResult r = measureRun(s, threads, 128, seconds);
      char label[64]; std::snprintf(label, sizeof label, "%s-t%d", sc.name, threads);
      printRun(label, r);
      json row = runJson(label, r);
      row["muted"] = sc.mutes; row["bypassFx"] = sc.bypass; row["plugins"] = plugins;
      summary.push_back(row);
      writeJson(std::string(outDir) + "/" + label + ".json", row);
      g_blockCount.store(0); g_blockOverflow.store(0);
    }
    for (int block : {64, 128, 256, 512}) {
      RunResult r = measureRun(song, threads, block, seconds);
      char label[64]; std::snprintf(label, sizeof label, "b-t%d-%d", threads, block);
      printRun(label, r);
      json row = runJson(label, r);
      row["muted"] = json::array(); row["bypassFx"] = false; row["plugins"] = plugins;
      row["pluginCallsPerCallback"] = block / 128.0;
      row["expectedPluginCalls"] = (double)r.timing.framesTotal / 128.0 * 4.0;
      summary.push_back(row);
      writeJson(std::string(outDir) + "/" + label + ".json", row);
      g_blockCount.store(0); g_blockOverflow.store(0);
    }
  }
  writeJson(std::string(outDir) + "/summary.json", summary);
  std::fprintf(stderr, "S9 measure: %zu scenarios written to %s\n", summary.size(), outDir);
  return 0;
}

// ================================================================ --covers: C1-C4 capture evidence

int runCovers(const char* outDir, const char* delayPath) {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  if (!synth.is_object() || !gain.is_object()) { std::fprintf(stderr, "S9 covers: test plugins not found\n"); return 1; }
  json out = {{"sampleRate", kSr}, {"backend", "null"}};

  // C1+C4: long note, mute at 1 s, unmute at 2 s, 3 s capture; serial and M5(4).
  {
    const json song = longNoteSong(synth);
    const int64_t frames = 144000;
    CaptureResult ref = runCapture(song, 0, 128, frames, {});
    CaptureResult mut = runCapture(song, 0, 128, frames, {{48000, "lead", true}, {96000, "lead", false}});
    CaptureResult mutM5 = runCapture(song, 4, 128, frames, {{48000, "lead", true}, {96000, "lead", false}});
    out["c1c4Serial"] = analyzeMuteResume(ref, mut, 48000, 96000);
    out["c1c4M5"] = analyzeMuteResume(ref, mutM5, 48000, 96000);
    out["c1c4Serial"]["error"] = ref.error.empty() ? mut.error : ref.error;
    writeCaptureRaw(std::string(outDir) + "/c1-reference.f32", ref);
    writeCaptureRaw(std::string(outDir) + "/c1-mute-serial.f32", mut);
    writeCaptureRaw(std::string(outDir) + "/c1-mute-m5.f32", mutM5);
    std::fprintf(stderr, "S9 covers C1+C4 serial: %s\n", out["c1c4Serial"].dump().c_str());
    std::fprintf(stderr, "S9 covers C1+C4 m5: %s\n", out["c1c4M5"].dump().c_str());
  }

  // C2: dormancy bit-compare + latency equality (kick muted in both captures).
  {
    const int64_t frames = 48000;
    CaptureResult with = runCapture(sidechainSong(synth, gain, true, true), 0, 128, frames, {});
    CaptureResult without = runCapture(sidechainSong(synth, gain, false, true), 0, 128, frames, {});
    const bool identical = with.frames == without.frames &&
        std::memcmp(with.L.data(), without.L.data(), (size_t)with.frames * 4) == 0 &&
        std::memcmp(with.R.data(), without.R.data(), (size_t)with.frames * 4) == 0;
    out["c2"] = {{"bitIdenticalWithAndWithoutField", identical},
                 {"latencyWithField", with.latency}, {"latencyWithoutField", without.latency},
                 {"peak", with.peak}, {"error", with.error.empty() ? without.error : with.error}};
    std::fprintf(stderr, "S9 covers C2: %s\n", out["c2"].dump().c_str());
  }

  // C3: latency invariance across the mute states (rebuilds).
  {
    TestHost host;
    Server server(makeMiniaudioOutput, kSr, &host);
    server.execute("project.load", {{"project", referenceSong(synth, gain)}});
    server.execute("engine.start", {{"backend", "null"}, {"blockSize", 128}, {"liveThreads", 0}});
    json states = json::array();
    states.push_back({{"state", "unmuted"}, {"latency", server.execute("engine.stats", json::object())["latency"]}});
    for (const char* id : {"kick", "bass", "lead"}) {
      server.execute("track.set", {{"trackId", id}, {"patch", {{"mute", true}}}});
      states.push_back({{"state", std::string("mute-") + id}, {"latency", server.execute("engine.stats", json::object())["latency"]}});
    }
    for (const char* id : {"kick", "bass", "lead"}) {
      server.execute("track.set", {{"trackId", id}, {"patch", {{"mute", false}}}});
      states.push_back({{"state", std::string("unmute-") + id}, {"latency", server.execute("engine.stats", json::object())["latency"]}});
    }
    server.execute("engine.stop", json::object());
    out["c3"] = states;
    std::fprintf(stderr, "S9 covers C3: %s\n", states.dump().c_str());
  }

  // C1 probe with the out-of-repo CoreTestDelay (480-sample line, reports 480 latency; gather adds 128).
  if (delayPath && *delayPath) {
    const json delay = plugins::scanFile(delayPath)[0];
    if (!delay.is_object()) { std::fprintf(stderr, "S9 covers: delay plugin not found at %s\n", delayPath); return 1; }
    const json song = delayTailSong(synth, delay);
    const int64_t frames = 48000;  // 1 s; note 0-0.5 s, tail out by 25568 samples
    CaptureResult r1 = runCapture(song, 0, 128, frames, {});                                  // reference
    CaptureResult r2 = runCapture(song, 0, 128, frames, {{12000, "src", true}, {36000, "src", false}});  // mute over the tail
    CaptureResult r3 = runCapture(song, 0, 128, frames, {{12000, "src", true}});              // mute, stay muted
    // Tail window: the note's last sample (24000) + synth release (960) leaves the delay at 24960+608=25568;
    // the line's stored signal starts leaving at 24000+128=24128.
    const int64_t tailFrom = 24128, tailTo = 25568;
    json j = {{"plugin", delay.value("name", "?")}, {"file", delay.value("file", "?")},
              {"latency", r1.latency},
              {"tailRmsReference", rms(r1.L, tailFrom, tailTo)},
              {"tailRmsMutedOverTail", rms(r2.L, tailFrom, tailTo)},
              {"tailRmsStayMuted", rms(r3.L, tailFrom, tailTo)},
              {"postUnmuteRmsReference", rms(r1.L, 36256, frames)},
              {"postUnmuteRmsMuteRun", rms(r2.L, 36256, frames)},
              {"errors", json::array({r1.error, r2.error, r3.error})}};
    // The decisive suspend-precondition datum: after unmuting at 0.75 s the mute run is SILENT — the delay
    // line drained WHILE muted (processing continued). A suspended track would instead release a stale
    // tail here, time-shifted by the suspend duration.
    j["tailDrainedWhileMuted"] = rms(r2.L, 36256, frames) < 1e-5 && rms(r1.L, tailFrom, tailTo) > 1e-4;
    out["c1Delay"] = j;
    writeCaptureRaw(std::string(outDir) + "/c1-delay-reference.f32", r1);
    writeCaptureRaw(std::string(outDir) + "/c1-delay-mute-over-tail.f32", r2);
    std::fprintf(stderr, "S9 covers C1 delay: %s\n", j.dump().c_str());
    // C3 with a real declared plugin latency: 128 (synth gather) + 480+128 (delay) = 736, invariant under mute.
    TestHost host;
    Server server(makeMiniaudioOutput, kSr, &host);
    server.execute("project.load", {{"project", song}});
    server.execute("engine.start", {{"backend", "null"}, {"blockSize", 128}, {"liveThreads", 0}});
    const json l0 = server.execute("engine.stats", json::object())["latency"];
    server.execute("track.set", {{"trackId", "src"}, {"patch", {{"mute", true}}}});
    const json l1 = server.execute("engine.stats", json::object())["latency"];
    server.execute("track.set", {{"trackId", "src"}, {"patch", {{"mute", false}}}});
    const json l2 = server.execute("engine.stats", json::object())["latency"];
    server.execute("engine.stop", json::object());
    out["c3Delay"] = {{"unmuted", l0}, {"muted", l1}, {"unmutedAgain", l2}};
    std::fprintf(stderr, "S9 covers C3 delay: %s\n", out["c3Delay"].dump().c_str());
  } else {
    out["c1Delay"] = "skipped: no delay plugin path given (out-of-repo CoreTestDelay build)";
  }
  writeJson(std::string(outDir) + "/covers.json", out);
  std::fprintf(stderr, "S9 covers: written to %s/covers.json\n", outDir);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  plugins::Runtime juce;
  return plugins::runWithMessageLoop([&] {
    if (argc > 2 && std::strcmp(argv[1], "--measure") == 0)
      return runMeasure(argv[2], argc > 3 ? std::atof(argv[3]) : 3.0);
    if (argc > 2 && std::strcmp(argv[1], "--covers") == 0)
      return runCovers(argv[2], argc > 3 ? argv[3] : nullptr);
    return runAll(argc, argv);
  });
}
