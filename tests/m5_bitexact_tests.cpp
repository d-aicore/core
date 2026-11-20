// S6 (engine RT proofs, owner round-2 item 6): the M5 live multicore path must sound BIT-EXACT like the serial
// path (quality bar 4: "the multicore path proven identical to the serial path"), measured with deterministic
// test plugins through the real VST3 host — not with fakes, and not by counters alone.
//
// Harness: the serve architecture without the protocol thread — in-process Server + JuceHost + null audio
// device (main.cpp serve: Runtime + runWithMessageLoop + EnginePlugins). The capture seam is the EXISTING
// Engine::startCapture/captureL/captureR (engine.hpp): the audio thread copies the mixed output AFTER
// Graph::process (which dispatches to Graph::processLive when the live workers are armed, graph.cpp) and
// BEFORE the ear guard (engine.cpp), so the capture is the live path's own samples, bit for bit.
//
// Parallel-usage proof: RtWorkerPool::workerJobs() (jobs executed by the worker threads, not the audio
// thread's take()) must advance while the capture runs — a configured pool that never ran a worker job is
// NOT the parallel path. engine.stats (liveThreads/liveFallback/liveStalls/callbacks) corroborates.
//
// M5 stays OFF by default (D51): every parallel run opts in explicitly with engine.start {liveThreads: n};
// an RAII guard clears CORE_LIVE_THREADS for the whole suite so the environment cannot opt in behind it
// (the same guard as the FL-1 regression in engine_tests.cpp). The default is never changed.
//
// The live plugin block is compile-time 128 (juce_host.cpp kPluginBlock); the ENGINE callback block varies
// (128/333/512/1024 here, all under the live cap kMaxLiveFrames = 4096). The reset watchdog's 2 s bound is
// measured with timing evidence in engine_process_tests.cpp (serve, CORE_TEST_FAULT seams).
//
// Run: m5_bitexact_tests [part of a name]
#include <algorithm>
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
const int64_t kCaptureFrames = 48000;  // 1 s = 2 beats at 120 BPM: every note of the song sounds inside it

// D51 guard: saves CORE_LIVE_THREADS, clears it (no opt-in from the environment), restores it on destruction
// even on exception — the FL-1 pattern (engine_tests.cpp). The suite's parallel runs opt in by argument only.
struct LiveEnvGuard {
  std::string saved; bool had = false;
  LiveEnvGuard() {
#ifdef _MSC_VER
    char* buf = nullptr; size_t len = 0;
    if (_dupenv_s(&buf, &len, "CORE_LIVE_THREADS") == 0 && buf) { had = true; saved = buf; free(buf); }
#else
    if (const char* e = std::getenv("CORE_LIVE_THREADS")) { had = true; saved = e; }
#endif
    set("");
  }
  void set(const char* v) {
#ifdef _MSC_VER
    _putenv_s("CORE_LIVE_THREADS", v);
#else
    if (*v) setenv("CORE_LIVE_THREADS", v, 1); else unsetenv("CORE_LIVE_THREADS");
#endif
  }
  ~LiveEnvGuard() { set(had ? saved.c_str() : ""); }
};

// The server's plugin host, exactly as core-engine serve wraps it (main.cpp EnginePlugins): the real JUCE
// VST3 host, one instance per track/effect, created on the message thread.
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

// SHA-256 (FIPS 180-4) of the capture's raw float bytes, L then R: a compact fingerprint for the evidence
// table. Bit-exactness itself is decided by memcmp, never by the hash alone.
std::string sha256Hex(const std::vector<float>& L, const std::vector<float>& R) {
  static const uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
  std::vector<uint8_t> b(L.size() * 4 + R.size() * 4);
  std::memcpy(b.data(), L.data(), L.size() * 4);
  std::memcpy(b.data() + L.size() * 4, R.data(), R.size() * 4);
  const uint64_t bits = (uint64_t)b.size() * 8;
  b.push_back(0x80);
  while (b.size() % 64 != 56) b.push_back(0);
  for (int i = 0; i < 8; ++i) b.push_back((uint8_t)(bits >> (56 - 8 * i)));
  uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  for (size_t off = 0; off < b.size(); off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (uint32_t)b[off + 4 * i] << 24 | (uint32_t)b[off + 4 * i + 1] << 16 | (uint32_t)b[off + 4 * i + 2] << 8 | (uint32_t)b[off + 4 * i + 3];
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const uint32_t ch = (e & f) ^ (~e & g);
      const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
      const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const uint32_t maj = (a & bb) ^ (a & c) ^ (bb & c);
      const uint32_t t2 = S0 + maj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = bb; bb = a; a = t1 + t2;
    }
    h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  char out[65];
  for (int i = 0; i < 8; ++i) std::snprintf(out + 8 * i, 9, "%08x", h[i]);
  return std::string(out, 64);
}

// Bit-exact comparison (memcmp on the raw float bytes) plus, when different, the characterization the
// owner asks for: first differing sample, how many, and the largest |delta|.
struct BitDiff {
  bool identical = true;
  int64_t firstAt = -1;   // sample index in L, or L.size() + index in R
  uint64_t count = 0;
  float maxDelta = 0;
};
BitDiff bitCompare(const std::vector<float>& aL, const std::vector<float>& aR,
                   const std::vector<float>& bL, const std::vector<float>& bR) {
  BitDiff d;
  d.identical = aL.size() == bL.size() && aR.size() == bR.size() &&
                std::memcmp(aL.data(), bL.data(), aL.size() * 4) == 0 &&
                std::memcmp(aR.data(), bR.data(), aR.size() * 4) == 0;
  if (d.identical) return d;
  auto scan = [&](const std::vector<float>& a, const std::vector<float>& b, int64_t base) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
      uint32_t ba, bb2;
      std::memcpy(&ba, &a[i], 4); std::memcpy(&bb2, &b[i], 4);
      if (ba == bb2) continue;
      if (d.firstAt < 0) d.firstAt = base + (int64_t)i;
      ++d.count;
      d.maxDelta = std::max(d.maxDelta, std::fabs(a[i] - b[i]));
    }
  };
  scan(aL, bL, 0);
  scan(aR, bR, (int64_t)aL.size());
  return d;
}

json note(int64_t tick, int64_t length, int pitch, double velocity) {
  return {{"tick", tick}, {"length", length}, {"pitch", pitch}, {"velocity", velocity}};
}

// The deterministic song: kick + bass + lead notes through CoreTestSynth (a sine per note, fixed params),
// the bass through a CoreTestGain insert (multiplies by its gain). Fixed ticks/pitches/velocities, no
// automation, no modulators, no randomness: the same input must give the same samples on every path.
// 120 BPM, 960 ticks/beat: the capture (2 beats) holds every note; the song is 16 s so nothing loops.
json m5Song(const json& synth, const json& gain) {
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
  return {{"format", kProjectFormat}, {"version", 2}, {"name", "s6 m5 bit-exact"}, {"bpm", 120.0},
          {"lengthTicks", 30720}, {"masterGainDb", 0.0}, {"tracks", json::array({kick, bass, lead})}};
}

struct RunResult {
  std::vector<float> L, R;
  uint64_t workerJobs = 0;   // jobs the WORKER threads ran during the capture (0 when serial: no pool)
  uint64_t callbacks = 0;
  int liveThreads = -1;
  bool liveFallback = false;
  uint64_t liveStalls = 0;
  int deviceBlock = 0;
  double wallMs = 0;
  float peak = 0;
};

// One full run: fresh host (fresh plugin instances), fresh engine, the song from sample 0, capture of
// kCaptureFrames through the live path's own output. liveThreads is the explicit opt-in (0 = serial, D51).
RunResult runOnce(const json& song, int liveThreads, int block) {
  RunResult r;
  const auto t0 = std::chrono::steady_clock::now();
  TestHost host;
  Server server(makeMiniaudioOutput, kSr, &host);
  const json loaded = server.execute("project.load", {{"project", song}});
  CHECK(loaded.value("missing", json::array()).empty());  // every test plugin loaded, or nothing is tested
  const json start = server.execute("engine.start", {{"backend", "null"}, {"blockSize", block}, {"liveThreads", liveThreads}});
  r.deviceBlock = start.value("blockSize", 0);
  Engine& e = server.engine();
  CHECK_EQ(e.liveThreads(), liveThreads);  // the opt-in took effect (and serial really has no pool)
  if (liveThreads == 0) CHECK(e.livePool() == nullptr);
  e.startCapture(kCaptureFrames);
  server.execute("engine.resetStats", json::object());
  const uint64_t jobs0 = e.livePool() ? e.livePool()->workerJobs() : 0;
  server.execute("transport.play", json::object());
  // The null device runs at wall pace; bound the wait so a dead device fails the test instead of hanging it.
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (e.captured() < kCaptureFrames && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(5ms);
  e.stopCapture();
  const uint64_t jobs1 = e.livePool() ? e.livePool()->workerJobs() : 0;
  const json stats = server.execute("engine.stats", json::object());
  r.workerJobs = jobs1 - jobs0;
  r.callbacks = stats.value("callbacks", (uint64_t)0);
  r.liveThreads = stats.value("liveThreads", -1);
  r.liveFallback = stats.value("liveFallback", false);
  r.liveStalls = stats.value("liveStalls", (uint64_t)0);
  CHECK_EQ(e.captured(), kCaptureFrames);
  r.L.assign(e.captureL().begin(), e.captureL().begin() + (size_t)e.captured());
  r.R.assign(e.captureR().begin(), e.captureR().begin() + (size_t)e.captured());
  for (size_t i = 0; i < r.L.size(); ++i) r.peak = std::max({r.peak, std::fabs(r.L[i]), std::fabs(r.R[i])});
  server.execute("transport.stop", json::object());
  server.execute("engine.stop", json::object());
  r.wallMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return r;
}

void printRow(const char* label, int block, const RunResult& r, const BitDiff* vsSerial) {
  std::fprintf(stderr, "S6 %s block=%d threads=%d deviceBlock=%d sha256=%s workerJobs=%llu callbacks=%llu stalls=%llu fallback=%d peak=%.4f wall=%.0fms",
               label, block, r.liveThreads, r.deviceBlock, sha256Hex(r.L, r.R).c_str(),
               (unsigned long long)r.workerJobs, (unsigned long long)r.callbacks, (unsigned long long)r.liveStalls,
               (int)r.liveFallback, r.peak, r.wallMs);
  if (vsSerial)
    std::fprintf(stderr, " bitExact=%d firstDiff=%lld count=%llu maxDelta=%.9g", (int)vsSerial->identical,
                 (long long)vsSerial->firstAt, (unsigned long long)vsSerial->count, vsSerial->maxDelta);
  std::fprintf(stderr, "\n");
}

}  // namespace

// The matrix: serial(0) vs M5(2) vs M5(4) at four callback block sizes, with the serial-vs-serial
// determinism control FIRST (if the plugins were non-deterministic nothing below could be proven).
TEST("M5 bit-exact: serial == live(2) == live(4) at blocks 128/333/512/1024, deterministic plugins (S6)") {
  LiveEnvGuard env;  // D51: no opt-in from the environment; every parallel run below opts in by argument
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  CHECK(synth.is_object());
  CHECK(gain.is_object());
  if (!synth.is_object() || !gain.is_object()) return;
  const json song = m5Song(synth, gain);

  RunResult serialRef[4];
  const int blocks[4] = {128, 333, 512, 1024};
  for (int bi = 0; bi < 4; ++bi) {
    const int block = blocks[bi];
    // Determinism control: two serial runs, fresh instances each, must be bit-identical.
    RunResult s1 = runOnce(song, 0, block);
    RunResult s2 = runOnce(song, 0, block);
    serialRef[bi] = s1;
    CHECK(s1.peak > 0.05f);  // the song really sounds (synth through the gain insert), not silence
    CHECK_EQ(s1.workerJobs, (uint64_t)0);  // serial: no pool, no worker jobs
    const BitDiff det = bitCompare(s1.L, s1.R, s2.L, s2.R);
    printRow("serial-control", block, s2, &det);
    CHECK(det.identical);  // the plugins are deterministic: the matrix below is meaningful
    if (!det.identical) continue;
    // The parallel runs: same song, same block, explicit opt-in.
    for (int threads : {2, 4}) {
      RunResult p = runOnce(song, threads, block);
      const BitDiff d = bitCompare(s1.L, s1.R, p.L, p.R);
      printRow("m5", block, p, &d);
      CHECK_EQ(p.liveThreads, threads);
      CHECK(!p.liveFallback);          // no stall fell back to serial mid-capture
      CHECK_EQ(p.liveStalls, (uint64_t)0);
      CHECK(p.callbacks > (uint64_t)0);
      CHECK(p.workerJobs > (uint64_t)0);  // PROOF the workers really played (not just configured)
      CHECK(d.identical);                 // THE bit-exact requirement (quality bar 4)
    }
  }
  // Bonus control (quality bar 5): the serial path itself is block-size independent — the plugin block is
  // compile-time 128 (juce_host.cpp), so the callback chunking must not change a single sample.
  for (int bi = 1; bi < 4; ++bi) {
    const BitDiff d = bitCompare(serialRef[0].L, serialRef[0].R, serialRef[bi].L, serialRef[bi].R);
    std::fprintf(stderr, "S6 serial block-independence 128 vs %d: bitExact=%d firstDiff=%lld maxDelta=%.9g\n",
                 blocks[bi], (int)d.identical, (long long)d.firstAt, d.maxDelta);
    CHECK(d.identical);
  }
}

// The parallel path with REAL plugins, exercised to its deadline: a worker made late on purpose must stall
// the graph (silence + serial fallback until re-arm) — a serial graph can never stall, so the stall itself
// proves the capture above ran on the workers. Same mechanism as the M5 re-arm test (engine_tests.cpp),
// here through the real VST3 host instead of fakes.
TEST("M5: a real-plugin graph runs on the workers — a late worker stalls it (S6)") {
  LiveEnvGuard env;
  const json synth = plugins::scanFile(CORE_TEST_SYNTH_VST3)[0];
  const json gain = plugins::scanFile(CORE_TEST_GAIN_VST3)[0];
  CHECK(synth.is_object());
  CHECK(gain.is_object());
  if (!synth.is_object() || !gain.is_object()) return;
  TestHost host;
  Server server(makeMiniaudioOutput, kSr, &host);
  const json loaded = server.execute("project.load", {{"project", m5Song(synth, gain)}});
  CHECK(loaded.value("missing", json::array()).empty());
  server.execute("engine.start", {{"backend", "null"}, {"blockSize", 256}, {"liveThreads", 2}});
  Engine& e = server.engine();
  CHECK(e.livePool() != nullptr);
  server.execute("transport.play", json::object());
  std::this_thread::sleep_for(200ms);  // ~40 callbacks: the workers play real plugin tracks
  const uint64_t jobs = e.livePool()->workerJobs();
  std::fprintf(stderr, "S6 late-worker: workerJobs after 200 ms of real-plugin playback = %llu\n", (unsigned long long)jobs);
  CHECK(jobs > (uint64_t)0);
  // 50 ms per job >> the deadline (2 buffers of 256/48000 s ~ 10.7 ms): the next callback misses it.
  e.livePool()->testDelayNs = 50000000;
  std::this_thread::sleep_for(150ms);
  const json stats = server.execute("engine.stats", json::object());
  std::fprintf(stderr, "S6 late-worker: liveStalls=%llu liveStallBlocks=%llu\n",
               (unsigned long long)stats.value("liveStalls", (uint64_t)0),
               (unsigned long long)stats.value("liveStallBlocks", (uint64_t)0));
  CHECK(stats.value("liveStalls", (uint64_t)0) >= (uint64_t)1);  // a serial graph never stalls
  e.livePool()->testDelayNs = 0;
  for (int i = 0; i < 1000 && !e.livePool()->idle(); ++i) std::this_thread::sleep_for(1ms);
  CHECK(e.livePool()->idle());
  std::this_thread::sleep_for(50ms);  // the recovery block runs on the device thread
  CHECK(e.liveFallback());  // serial until a publish re-arms (engine.cpp, the M5 re-arm contract)
  server.execute("transport.stop", json::object());
  server.execute("engine.stop", json::object());
}

int main(int argc, char** argv) {
  // JUCE ready on this thread (it becomes the message thread); the cases run on a thread of their own
  // while this one dispatches the message loop — exactly core-engine serve's architecture (main.cpp).
  plugins::Runtime juce;
  return plugins::runWithMessageLoop([&] { return runAll(argc, argv); });
}
