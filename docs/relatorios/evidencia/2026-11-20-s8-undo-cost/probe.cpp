// S8 undo-cost measurement probe (measurement only; no repo code changed).
// Links core_domain from D:\Core-build\s8\build. Deterministic reference documents,
// QPC wall times, process memory counters, 3 repeats, raw CSV output.
// Build (vcvars64): cl /std:c++20 /O2 /EHsc /utf-8 /MT /I <repo>\app\domain\src
//   /I <repo>\engine\third_party probe.cpp /link <build>\app\domain\core_domain.lib psapi.lib
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include "document.hpp"
#include "history.hpp"

using core::doc::Document;
using core::doc::History;
using core::doc::json;
using core::doc::toJson;
using core::doc::sameContent;

static volatile size_t g_sink = 0;
static FILE* g_csv = nullptr;

struct Timer {
  LARGE_INTEGER f, a, b;
  Timer() { QueryPerformanceFrequency(&f); }
  void start() { QueryPerformanceCounter(&a); }
  double stopMs() { QueryPerformanceCounter(&b); return double(b.QuadPart - a.QuadPart) * 1000.0 / double(f.QuadPart); }
};

static void row(const char* cfg, int rep, const char* metric, double v, const char* unit, const char* note = "") {
  std::fprintf(g_csv, "%s,%d,%s,%.6f,%s,%s\n", cfg, rep, metric, v, unit, note);
  std::fflush(g_csv);
  std::printf("%s r%d %-28s %12.3f %s %s\n", cfg, rep, metric, v, unit, note);
  std::fflush(stdout);
}

struct Mem { PROCESS_MEMORY_COUNTERS_EX c{}; };
static Mem mem() {
  Mem m; m.c.cb = sizeof(m.c);
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m.c), sizeof(m.c));
  return m;
}
static double mb(size_t bytes) { return double(bytes) / (1024.0 * 1024.0); }

// Deterministic fake plugin state: exactly n chars of base64 alphabet, per-track identity in char 0
// (same shape as tests/engine_process_tests.cpp stateSong: 318128 chars, first char 'B'+i%20).
static std::string makeState(size_t n, int track) {
  static const char* ab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string s(n, 'A');
  uint32_t x = 0x12345678u + uint32_t(track) * 2654435761u;
  for (size_t k = 0; k < n; ++k) { x = x * 1664525u + 1013904223u; s[k] = ab[(x >> 24) & 63]; }
  if (n) s[0] = char('B' + track % 20);
  return s;
}

static Document makeDoc(int tracks, size_t stateBytes) {
  Document d;
  d.name = "s8-probe";
  d.lengthTicks = 64 * 4 * core::doc::kPpq;
  for (int i = 0; i < tracks; ++i) {
    core::doc::Track t;
    t.id = "t" + std::to_string(i);
    t.name = "Synth " + std::to_string(i);
    t.instrument.kind = "plugin";
    t.instrument.plugin = {{"format", "vst3"}, {"id", "probe-synth"}, {"name", "ProbeSynth"},
                           {"vendor", "S8"}, {"version", 1}, {"file", ""},
                           {"state", makeState(stateBytes, i)}};
    core::doc::Pattern p;
    p.id = "p0"; p.name = "pattern"; p.lengthTicks = 4 * core::doc::kPpq;
    for (int k = 0; k < 64; ++k) {
      core::doc::Note n;
      n.id = "n" + std::to_string(k);
      n.tick = k * 60; n.length = 120; n.pitch = 36 + (k % 12); n.velocity = 0.8;
      p.notes.push_back(std::move(n));
    }
    t.patterns.push_back(std::move(p));
    for (int c = 0; c < 2; ++c) {
      core::doc::Clip cl;
      cl.id = "c" + std::to_string(c); cl.patternId = "p0";
      cl.startTick = c * 16 * core::doc::kPpq; cl.lengthTicks = 16 * core::doc::kPpq;
      t.clips.push_back(std::move(cl));
    }
    core::doc::Automation a; a.param = "gainDb";
    for (int k = 0; k < 32; ++k) { core::doc::AutomationPoint pt; pt.tick = k * 480; pt.value = (k % 10) / 10.0; a.points.push_back(pt); }
    t.automation.push_back(std::move(a));
    core::doc::Automation a2; a2.param = "vst:probe:macro1";
    for (int k = 0; k < 16; ++k) { core::doc::AutomationPoint pt; pt.tick = k * 960; pt.value = (k % 8) / 8.0; a2.points.push_back(pt); }
    t.automation.push_back(std::move(a2));
    d.tracks.push_back(std::move(t));
  }
  return d;
}

// Edit i: one note velocity, distinct (track,note) pair for i in [0,60) at every tested track count.
static void applyEdit(Document& d, int i) {
  const int N = int(d.tracks.size());
  auto& t = d.tracks[i % N];
  auto& n = t.patterns[0].notes[(i / N) % 64];
  n.velocity = 0.05 + 0.01 * ((i * 7) % 90);
}
static Document expected(const Document& base, int k) {
  Document d = base;
  for (int i = 0; i < k; ++i) applyEdit(d, i);
  return d;
}

// Estimate of the bytes a whole-Document deep copy moves: exact for strings/POD vectors,
// json fields approximated by their canonical dump size (nlohmann node overhead NOT counted).
static size_t jsonBytes(const json& j) { return j.dump().size(); }
static size_t estCopyBytes(const Document& d) {
  size_t b = sizeof(Document) + d.name.size();
  b += jsonBytes(d.masterEffects) + jsonBytes(d.ui) + jsonBytes(d.transportExtra) + jsonBytes(d.masterExtra) + jsonBytes(d.extra);
  for (const auto& a : d.masterAutomation) { b += sizeof(a) + a.param.size() + a.points.size() * sizeof(core::doc::AutomationPoint) + jsonBytes(a.extra); }
  for (const auto& t : d.tracks) {
    b += sizeof(t) + t.id.size() + t.name.size() + t.color.size();
    b += t.instrument.kind.size() + t.instrument.name.size() + jsonBytes(t.instrument.plugin) + jsonBytes(t.instrument.extra);
    b += jsonBytes(t.mix.extra) + t.mix.output.size();
    for (const auto& s : t.mix.sends) b += sizeof(s) + s.id.size() + s.to.size() + jsonBytes(s.extra);
    for (const auto& p : t.patterns) {
      b += sizeof(p) + p.id.size() + p.name.size() + jsonBytes(p.extra) + p.notes.size() * sizeof(core::doc::Note);
      for (const auto& n : p.notes) b += n.id.size() + jsonBytes(n.extra) + jsonBytes(n.roll.extra);
    }
    for (const auto& c : t.clips) b += sizeof(c) + c.id.size() + c.patternId.size() + jsonBytes(c.extra);
    for (const auto& a : t.automation) b += sizeof(a) + a.param.size() + a.points.size() * sizeof(core::doc::AutomationPoint) + jsonBytes(a.extra);
    for (const auto& a : t.audioClips) b += sizeof(a) + a.id.size() + a.assetId.size() + jsonBytes(a.extra);
    b += jsonBytes(t.effects) + jsonBytes(t.extra);
  }
  for (const auto& s : d.sections) b += sizeof(s) + s.id.size() + s.name.size() + s.color.size() + jsonBytes(s.extra);
  for (const auto& a : d.assets) b += sizeof(a) + a.id.size() + a.name.size() + a.path.size() + a.sha256.size() + jsonBytes(a.extra);
  return b;
}

static size_t countStates(const History& h) {
  size_t n = 0;
  h.forEachState([&](const Document&) { ++n; });
  return n;
}

struct Stats { double mean = 0, max = 0, total = 0; };
static Stats stats(const std::vector<double>& v) {
  Stats s;
  for (double x : v) { s.total += x; if (x > s.max) s.max = x; }
  if (!v.empty()) s.mean = s.total / double(v.size());
  return s;
}

static void runConfig(int tracks, size_t stateBytes, int rep, Timer& T) {
  char cfg[64];
  std::snprintf(cfg, sizeof(cfg), "t%d_s%zu", tracks, stateBytes);
  const Document doc0 = makeDoc(tracks, stateBytes);
  const size_t price = toJson(doc0).dump().size();
  const size_t copyEst = estCopyBytes(doc0);
  row(cfg, rep, "price_bytes", double(price), "B", "toJson+dump size (what sizeOf charges)");
  row(cfg, rep, "est_copy_bytes", double(copyEst), "B", "deep-copy estimate; json via dump size");

  // ---- component microbenchmarks (5 iterations each) ----
  std::vector<double> vJ, vD, vC, vS;
  Document twin = doc0;
  for (int it = 0; it < 5; ++it) {
    T.start(); { json j = toJson(doc0); g_sink += j.size(); } vJ.push_back(T.stopMs());
    T.start(); { std::string s = toJson(doc0).dump(); g_sink += s.size(); } vD.push_back(T.stopMs());
    T.start(); { Document b = doc0; g_sink += b.tracks.size(); } vC.push_back(T.stopMs());
    T.start(); { bool eq = sameContent(doc0, twin); g_sink += eq ? 1 : 0; } vS.push_back(T.stopMs());
  }
  const double tJ = stats(vJ).mean, tD = stats(vD).mean, tC = stats(vC).mean, tS = stats(vS).mean;
  row(cfg, rep, "toJson_ms", tJ, "ms", "json tree build only");
  row(cfg, rep, "toJson_dump_ms", tD, "ms", "tree + canonical string (sizeOf)");
  row(cfg, rep, "doc_copy_ms", tC, "ms", "whole-Document copy ctor");
  row(cfg, rep, "sameContent_ms", tS, "ms", "2x toJson + tree compare");

  // ---- 60 commits on a fresh History (default 64 MB / 1 GB limits) ----
  Mem m0 = mem();
  History h(doc0);
  std::vector<double> commits;
  for (int i = 0; i < 60; ++i) {
    Document next = h.current();
    applyEdit(next, i);
    T.start(); h.commit(next, "edit"); commits.push_back(T.stopMs());
  }
  Mem m1 = mem();
  const Stats cs = stats(commits);
  row(cfg, rep, "commit_mean_ms", cs.mean, "ms", "60 commits, constant label (cancel-check active)");
  row(cfg, rep, "commit_max_ms", cs.max, "ms");
  row(cfg, rep, "commit_total_ms", cs.total, "ms");
  const size_t statesAfterCommits = countStates(h);
  row(cfg, rep, "states_after_commits", double(statesAfterCommits), "count", "current + undo + redo");
  const double stackMB = mb(double(statesAfterCommits - 1) * price);
  row(cfg, rep, "priced_stack_mb", stackMB, "MB", "(states-1) x price_bytes");
  row(cfg, rep, "private_delta_commits_mb", mb(m1.c.PrivateUsage - m0.c.PrivateUsage), "MB", "heap growth over 60 commits");
  row(cfg, rep, "unpriced_gap_mb", mb(m1.c.PrivateUsage - m0.c.PrivateUsage) - stackMB, "MB", "current + saved_ tree + allocator overhead");

  // ---- undo traversal (60) with integrity checkpoints (outside timed regions) ----
  std::vector<double> undos;
  int integrityFail = 0, integrityChecks = 0;
  int u = 0;
  while (h.canUndo()) {
    T.start(); bool ok = h.undo(); double ms = T.stopMs();
    if (!ok) { ++integrityFail; break; }
    undos.push_back(ms);
    ++u;
    if (u == 15 || u == 30 || u == 45 || u == 60) {
      ++integrityChecks;
      if (!sameContent(h.current(), expected(doc0, 60 - u))) ++integrityFail;
    }
  }
  Mem m2 = mem();
  const Stats us = stats(undos);
  const int U = u;  // undos actually available (trim may have dropped the oldest steps)
  ++integrityChecks;
  if (!sameContent(h.current(), expected(doc0, 60 - U))) ++integrityFail;  // full-undo landing point
  row(cfg, rep, "undo_count", double(U), "count", "60 minus steps trimmed");
  row(cfg, rep, "undo_mean_ms", us.mean, "ms", "moves + sizeOf(toJson+dump) each");
  row(cfg, rep, "undo_max_ms", us.max, "ms");
  row(cfg, rep, "states_after_undo", double(countStates(h)), "count");
  row(cfg, rep, "private_after_undo_mb", mb(m2.c.PrivateUsage - m0.c.PrivateUsage), "MB");

  // ---- redo traversal (60) ----
  std::vector<double> redos;
  int r = 0;
  while (h.canRedo()) {
    T.start(); bool ok = h.redo(); double ms = T.stopMs();
    if (!ok) { ++integrityFail; break; }
    redos.push_back(ms);
    ++r;
    if (r == 15 || r == 30 || r == 45 || r == 60) {
      ++integrityChecks;
      // trim-aware: full undo landed on expected(60-U); r redos re-apply the last r edits
      if (!sameContent(h.current(), expected(doc0, 60 - U + r))) ++integrityFail;
    }
  }
  ++integrityChecks;
  if (!sameContent(h.current(), expected(doc0, 60))) ++integrityFail;  // full redo must restore the final state
  Mem m3 = mem();
  const Stats rs = stats(redos);
  row(cfg, rep, "redo_mean_ms", rs.mean, "ms");
  row(cfg, rep, "redo_max_ms", rs.max, "ms");
  row(cfg, rep, "states_after_redo", double(countStates(h)), "count");
  row(cfg, rep, "private_after_redo_mb", mb(m3.c.PrivateUsage - m0.c.PrivateUsage), "MB");
  row(cfg, rep, "peak_workingset_mb", mb(m3.c.PeakWorkingSetSize), "MB", "process-lifetime peak (cumulative)");
  row(cfg, rep, "integrity_checks", double(integrityChecks), "count", "sameContent vs deterministic replay");
  row(cfg, rep, "integrity_failures", double(integrityFail), "count");
  // blob integrity: track 0 state must survive the whole traversal byte-identical
  {
    const std::string want = makeState(stateBytes, 0);
    const std::string got = h.current().tracks[0].instrument.plugin.value("state", std::string());
    row(cfg, rep, "blob_intact", got == want ? 1.0 : 0.0, "bool");
  }

  // ---- gesture path ----
  {
    History hg(doc0);
    T.start(); hg.beginGesture("drag"); double beginMs = T.stopMs();
    std::vector<double> ups;
    for (int k = 0; k < 10; ++k) {
      Document next = hg.current();
      next.tracks[0].mix.gainDb = -0.1 * (k + 1);
      T.start(); hg.updateGesture(next); ups.push_back(T.stopMs());
    }
    T.start(); hg.endGesture(); double endMs = T.stopMs();
    row(cfg, rep, "gesture_begin_ms", beginMs, "ms", "gestureStart_ = current_ copy");
    row(cfg, rep, "gesture_update_mean_ms", stats(ups).mean, "ms", "current_ = next copy, no serialization");
    row(cfg, rep, "gesture_end_ms", endMs, "ms", "sameContent x2(+2) + sizeOf + trim");
    T.start(); bool mod = hg.modified(); double modMs = T.stopMs();  // first ask after revision: full toJson
    row(cfg, rep, "modified_first_ms", modMs, "ms", mod ? "dirty=true, 1x toJson" : "dirty=false, 1x toJson");
    hg.markSaved();
    T.start(); bool mod2 = hg.modified(); double mod2Ms = T.stopMs();
    row(cfg, rep, "modified_cached_ms", mod2Ms, "ms", mod2 ? "unexpected dirty" : "cached, no serialization");
  }

  // ---- ceiling behavior with a small ctor ceiling (10 steps worth) ----
  {
    const size_t cap = 10 * price;
    History hc(doc0, cap, cap);
    for (int i = 0; i < 12; ++i) { Document next = hc.current(); applyEdit(next, i); hc.commit(next, "edit"); }
    const size_t sc = countStates(hc);
    row(cfg, rep, "ceiling_states_after_12_commits", double(sc), "count", "cap=10xprice; hard trim on commit");
    row(cfg, rep, "ceiling_stack_bytes_after_commits", double(sc - 1) * price, "B", "vs cap");
    row(cfg, rep, "ceiling_cap_bytes", double(cap), "B");
    while (hc.canUndo()) hc.undo();
    const size_t su = countStates(hc);
    row(cfg, rep, "ceiling_states_after_full_undo", double(su), "count", "redo stack holds the steps");
    row(cfg, rep, "ceiling_stack_bytes_after_undo", double(su - 1) * price, "B", "conserved? <= cap means yes");
    row(cfg, rep, "ceiling_respected", (double(su - 1) * price <= cap) ? 1.0 : 0.0, "bool");
    while (hc.canRedo()) hc.redo();
  }
  // floor exception: ceiling below one step keeps 1 undo by design (history.hpp comment)
  {
    History hf(doc0, price / 2, price / 2);
    Document next = hf.current(); applyEdit(next, 0); hf.commit(next, "edit");
    row(cfg, rep, "floor_states_cap_half_step", double(countStates(hf)), "count", "kFloorSteps=1 keeps one undo above cap");
  }

  // ---- counterfactual: commit without serialization (2 copies + O(1) counter) ----
  const double predNoSerial = 2.0 * tC;
  row(cfg, rep, "pred_commit_no_serial_ms", predNoSerial, "ms", "2 x doc_copy; counter pricing ~0");
  row(cfg, rep, "serial_share_of_commit", cs.mean > 0 ? 1.0 - predNoSerial / cs.mean : 0.0, "frac", "1 - (2 copies / commit)");
  row(cfg, rep, "commit_over_copies", cs.mean > 0 ? cs.mean / (predNoSerial > 0 ? predNoSerial : 1) : 0.0, "x");
}

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "D:\\Core-build\\s8\\probe\\results.csv";
  g_csv = std::fopen(out, "w");
  if (!g_csv) { std::fprintf(stderr, "cannot open %s\n", out); return 1; }
  std::fprintf(g_csv, "config,rep,metric,value,unit,note\n");
  Timer T;
  struct Cfg { int tracks; size_t state; };
  const Cfg cfgs[] = {
      {4, 0}, {16, 0}, {32, 0}, {64, 0},
      {4, 318128}, {16, 318128}, {32, 65536}, {32, 318128}, {64, 318128}, {32, 1048576},
  };
  Mem mStart = mem();
  std::printf("S8 undo-cost probe: private at start %.1f MB\n", mb(mStart.c.PrivateUsage));
  for (const auto& c : cfgs)
    for (int rep = 1; rep <= 3; ++rep) {
      std::printf("---- t%d_s%zu rep %d ----\n", c.tracks, c.state, rep);
      runConfig(c.tracks, c.state, rep, T);
    }
  Mem mEnd = mem();
  std::printf("done. private at end %.1f MB, peak working set %.1f MB, sink=%zu\n",
              mb(mEnd.c.PrivateUsage), mb(mEnd.c.PeakWorkingSetSize), g_sink);
  std::fprintf(g_csv, "summary,0,private_end_mb,%.3f,MB,\n", mb(mEnd.c.PrivateUsage));
  std::fprintf(g_csv, "summary,0,peak_workingset_mb,%.3f,MB,\n", mb(mEnd.c.PeakWorkingSetSize));
  std::fclose(g_csv);
  return 0;
}