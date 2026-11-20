// S1 (DATA priority, owner-authorized): an export whose needed plugin failed to LOAD or failed DURING the
// render is REFUSED before anything is published, naming plugin/track/why; the previous export stays
// byte-identical and no "Exported" notice can fire. A bypassed unit is intentional silence, not a failure.
// Red measured at a9a0384 (coordinator-round2/s1-repro-notes.md, cases a-e): the unfixed engine published
// the degraded render — byte-identical to the healthy control — with ok:true / state "succeeded".
//
// The simpler offline path (audit2_tests pattern): RenderJobs is the app's render.start worker;
// renderProject is the CLI's synchronous render engine (Server::render calls it). Fake providers
// reproduce the two failure states exactly: get() -> nullptr with a why (load failure, graph.cpp
// loadPlugin) and a processor marked failed (the state runPlugin's exception/SEH catch leaves,
// graph.cpp:93-107). The real VST3 SEH crash (Core Test Crash) is covered end-to-end by
// cli_plugin_crash_render on the same merge label. No JUCE needed here: this suite runs in every build.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>
#include "support/check.hpp"
#include "core/graph.hpp"
#include "core/project.hpp"
#include "core/render_job.hpp"
#include "core/server.hpp"
#include "io/wav.hpp"

namespace fs = std::filesystem;
using core::json;

namespace {

// --- the fake plugins -----------------------------------------------------------------------------------------

// Healthy: a constant level, finite and audible (the green control must really sound).
struct HealthyProc final : core::PluginProcessor {
  void process(float* l, float* r, int n, const core::MidiNote*, int) override {
    for (int i = 0; i < n; ++i) { l[i] += 0.25f; r[i] += 0.25f; }
  }
  bool isInstrument() const override { return true; }
  int latencySamples() const override { return 0; }
};

// Crash: plays 0.1 s, then marks itself failed (Crash) and clears its buffer — the exact state the host's
// SEH catch leaves a real crashed plugin in (Core Test Crash crashes unconditionally 0.1 s into playback,
// app/selftest-plugins/TestPlugins.cpp:212-215). runPlugin never calls a failed processor again.
struct CrashProc final : core::PluginProcessor {
  int64_t played = 0;
  void process(float* l, float* r, int n, const core::MidiNote*, int) override {
    played += n;
    if (played >= 4800) {  // 0.1 s at 48 kHz
      markFailed(core::PluginProcessor::Failure::Crash);
      std::fill(l, l + n, 0.0f);
      std::fill(r, r + n, 0.0f);
      return;
    }
    for (int i = 0; i < n; ++i) { l[i] += 0.25f; r[i] += 0.25f; }
  }
  bool isInstrument() const override { return true; }
  int latencySamples() const override { return 0; }
};

// Throw: the exception variant of the same marking (runPlugin's catch(...), Failure::Exception).
struct ThrowProc final : core::PluginProcessor {
  void process(float*, float*, int, const core::MidiNote*, int) override {
    throw std::runtime_error("the plugin exploded");
  }
  bool isInstrument() const override { return true; }
  int latencySamples() const override { return 0; }
};

// One instance per get(), as the real offline host does (a new instance per export graph).
struct ScriptedProvider final : core::PluginProvider {
  std::map<std::string, std::string> script;  // key -> healthy | crash | throw | missing (default: healthy)
  std::shared_ptr<core::PluginProcessor> get(const std::string& key, const json&, double, bool, std::string* why) override {
    const auto it = script.find(key);
    const std::string role = it == script.end() ? "healthy" : it->second;
    if (role == "missing") { if (why) *why = "not found in no-such-plugin.vst3"; return nullptr; }
    if (role == "crash") return std::make_shared<CrashProc>();
    if (role == "throw") return std::make_shared<ThrowProc>();
    if (why) why->clear();
    return std::make_shared<HealthyProc>();
  }
  void keepOnly(const std::vector<std::string>&) override {}
};

// --- songs and helpers ----------------------------------------------------------------------------------------

json pluginRef(const std::string& name) {
  return {{"format", "vst3"}, {"id", "VST3-" + name}, {"name", name}, {"vendor", "Core"}, {"version", "1.0.0"}};
}
json track(const std::string& id, const std::string& pluginName, int pitch, const json& effects = json::array()) {
  return {{"id", id}, {"name", id}, {"instrument", "plugin"}, {"plugin", pluginRef(pluginName)},
          {"patternTicks", 3840}, {"notes", json::array({{{"tick", 0}, {"length", 960}, {"pitch", pitch}, {"velocity", 0.8}}})},
          {"effects", effects}};
}
json song(const json& tracks, const json& masterEffects = json::array()) {
  return {{"format", "core-project"}, {"version", 2}, {"name", "s1 export refusal"}, {"bpm", 120.0},
          {"lengthTicks", 3840}, {"masterGainDb", 0.0}, {"tracks", tracks}, {"masterEffects", masterEffects}};
}
json effect(const std::string& id, const std::string& pluginName, bool bypass) {
  return {{"type", "plugin"}, {"id", id}, {"plugin", pluginRef(pluginName)}, {"bypass", bypass}};
}

fs::path workDir(const std::string& name) {
  static const fs::path root = fs::temp_directory_path() / "core-s1-export-plugin-failure";
  const fs::path d = root / name;
  std::error_code ignored;
  fs::remove_all(d, ignored);
  fs::create_directories(d);
  return d;
}
std::string bytesOf(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
// A previous export with its own content (a constant level): byte comparison proves it was not replaced.
void seedWav(const fs::path& p, float level) {
  const std::vector<float> s(24000, level);
  core::writeWav(p.string(), s, s, 48000, 24);
}
core::RenderJobRequest request(const json& s, const std::string& target, core::PluginProvider* provider) {
  core::RenderJobRequest r;
  r.project = core::fromJson(s);
  r.path = target;
  r.sampleRate = 48000;
  r.bits = 24;
  r.tailSeconds = 0.2;
  r.plugins = provider;
  return r;
}
json waitForJob(core::RenderJobs& jobs, uint64_t id) {
  json status;
  for (int i = 0; i < 10000; ++i) {
    status = jobs.status(id);
    const std::string s = status.value("state", "");
    if (s == "succeeded" || s == "failed" || s == "cancelled") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return status;
}
// The refusal message of the CLI engine path ("" = it did not refuse: the defect).
std::string refusalOf(const std::function<void()>& fn) {
  try { fn(); } catch (const std::exception& e) { return e.what(); }
  return "";
}
bool names(const std::string& msg, const std::string& part) { return msg.find(part) != std::string::npos; }
// Only the seeded files may remain: no published export, no working debris, no .previous aside.
void onlyFiles(const fs::path& dir, const std::vector<std::string>& expected) {
  std::vector<std::string> seen;
  for (const auto& e : fs::directory_iterator(dir)) seen.push_back(e.path().filename().string());
  std::sort(seen.begin(), seen.end());
  std::vector<std::string> want = expected;
  std::sort(want.begin(), want.end());
  CHECK_EQ(seen.size(), want.size());
  for (size_t i = 0; i < std::min(seen.size(), want.size()); ++i) CHECK_EQ(seen[i], want[i]);
}

}  // namespace

// --- case 1: load failure -------------------------------------------------------------------------------------

TEST("S1 case 1: a needed plugin that cannot load refuses the export; the previous WAV stays byte-identical") {
  const fs::path dir = workDir("case1-loadfail");
  ScriptedProvider provider;
  provider.script["track:ghost"] = "missing";
  const json s = song(json::array({track("synth", "Core Test Synth", 60), track("ghost", "Ghost Missing Plugin", 64)}));
  // The app path (render.start -> RenderJobs).
  const fs::path target = dir / "app.wav";
  seedWav(target, 0.3f);
  const std::string before = bytesOf(target);
  core::RenderJobs jobs;
  const uint64_t id = jobs.start(request(s, target.string(), &provider))["jobId"].get<uint64_t>();
  const json st = waitForJob(jobs, id);
  CHECK_EQ(st.value("state", ""), std::string("failed"));
  const std::string err = st.value("error", "");
  CHECK(names(err, "ghost"));                 // the track
  CHECK(names(err, "Ghost Missing Plugin"));  // the plugin
  CHECK(names(err, "not found"));             // the why
  CHECK_EQ(bytesOf(target), before);          // the previous export, byte for byte
  onlyFiles(dir, {"app.wav"});                // nothing published, no working debris, no .previous
  // The CLI engine path (Server::render -> renderProject): the same refusal, before any write.
  const std::string msg = refusalOf([&] { core::renderProject(core::fromJson(s), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0); });
  CHECK(!msg.empty());
  CHECK(names(msg, "ghost") && names(msg, "Ghost Missing Plugin") && names(msg, "not found"));
}

// --- case 2: crash during render ------------------------------------------------------------------------------

TEST("S1 case 2: a plugin that crashes during the render refuses the export before publish; previous WAV intact") {
  const fs::path dir = workDir("case2-crash");
  ScriptedProvider provider;
  provider.script["track:crash"] = "crash";
  const json s = song(json::array({track("crash", "Core Test Crash", 60), track("synth", "Core Test Synth", 64)}));
  const fs::path target = dir / "app.wav";
  seedWav(target, 0.3f);
  const std::string before = bytesOf(target);
  core::RenderJobs jobs;
  const uint64_t id = jobs.start(request(s, target.string(), &provider))["jobId"].get<uint64_t>();
  const json st = waitForJob(jobs, id);
  CHECK_EQ(st.value("state", ""), std::string("failed"));
  const std::string err = st.value("error", "");
  CHECK(names(err, "track:crash"));        // the track (the graph's plugin key)
  CHECK(names(err, "Core Test Crash"));    // the plugin
  CHECK(names(err, "crash"));              // the why
  CHECK_EQ(bytesOf(target), before);
  onlyFiles(dir, {"app.wav"});
  const std::string msg = refusalOf([&] { core::renderProject(core::fromJson(s), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0); });
  CHECK(!msg.empty());
  CHECK(names(msg, "track:crash") && names(msg, "Core Test Crash") && names(msg, "crash"));
  // The exception variant marks the same failed state (runPlugin's catch): it refuses too.
  ScriptedProvider thrower;
  thrower.script["track:crash"] = "throw";
  const std::string msg2 = refusalOf([&] { core::renderProject(core::fromJson(s), 48000, 512, false, 0.2, nullptr, nullptr, &thrower, 0); });
  CHECK(!msg2.empty());
  CHECK(names(msg2, "track:crash") && names(msg2, "exception"));
  core::RenderJobs jobs2;
  const fs::path target2 = dir / "app-throw.wav";
  seedWav(target2, 0.3f);
  const std::string before2 = bytesOf(target2);
  const uint64_t id2 = jobs2.start(request(s, target2.string(), &thrower))["jobId"].get<uint64_t>();
  const json st2 = waitForJob(jobs2, id2);
  CHECK_EQ(st2.value("state", ""), std::string("failed"));
  CHECK(names(st2.value("error", ""), "exception"));
  CHECK_EQ(bytesOf(target2), before2);
}

// --- case 3: healthy control ----------------------------------------------------------------------------------

TEST("S1 case 3: a healthy song exports successfully with non-zero audio (green control)") {
  const fs::path dir = workDir("case3-healthy");
  ScriptedProvider provider;
  const json s = song(json::array({track("synth", "Core Test Synth", 60)}));
  const fs::path target = dir / "app.wav";
  core::RenderJobs jobs;
  const uint64_t id = jobs.start(request(s, target.string(), &provider))["jobId"].get<uint64_t>();
  const json st = waitForJob(jobs, id);
  CHECK_EQ(st.value("state", ""), std::string("succeeded"));
  CHECK(!st.contains("error"));                    // the succeeded JSON contract is unchanged
  CHECK(st.value("peak", 0.0) > 0.05);             // really sounds
  CHECK(fs::exists(target));
  CHECK_EQ(bytesOf(target).substr(0, 4), std::string("RIFF"));
  const core::RenderResult r = core::renderProject(core::fromJson(s), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0);
  CHECK(r.peak > 0.05f);
  bool finite = true;
  for (float x : r.L) finite = finite && std::isfinite(x);
  CHECK(finite);
}

// --- case 4: stems — the WHOLE set is refused ------------------------------------------------------------------

TEST("S1 case 4: stems with a crashed track refuse the WHOLE set; every pre-existing target stays byte-identical") {
  const fs::path dir = workDir("case4-stems");
  ScriptedProvider provider;
  provider.script["track:crash"] = "crash";
  const json s = song(json::array({track("crash", "Core Test Crash", 60), track("synth", "Core Test Synth", 64)}));
  const fs::path mix = dir / "mix.wav", stemCrash = dir / "mix-crash.wav", stemSynth = dir / "mix-synth.wav";
  seedWav(mix, 0.3f); seedWav(stemCrash, 0.4f); seedWav(stemSynth, 0.5f);
  const std::string beforeMix = bytesOf(mix), beforeCrash = bytesOf(stemCrash), beforeSynth = bytesOf(stemSynth);
  core::RenderJobRequest r = request(s, mix.string(), &provider);
  r.outputs = {{mix.string(), -1}, {stemCrash.string(), 0}, {stemSynth.string(), 1}};
  core::RenderJobs jobs;
  const uint64_t id = jobs.start(std::move(r))["jobId"].get<uint64_t>();
  const json st = waitForJob(jobs, id);
  CHECK_EQ(st.value("state", ""), std::string("failed"));
  CHECK(names(st.value("error", ""), "track:crash"));
  CHECK_EQ(bytesOf(mix), beforeMix);
  CHECK_EQ(bytesOf(stemCrash), beforeCrash);
  CHECK_EQ(bytesOf(stemSynth), beforeSynth);
  onlyFiles(dir, {"mix.wav", "mix-crash.wav", "mix-synth.wav"});  // no stem published, no debris
  // The CLI engine path with stems: the same whole-set refusal.
  const std::string msg = refusalOf([&] { core::renderProject(core::fromJson(s), 48000, 512, true, 0.2, nullptr, nullptr, &provider, 0); });
  CHECK(!msg.empty());
  CHECK(names(msg, "track:crash"));
}

// --- case 5: bypass is intentional silence, not failure ---------------------------------------------------------

TEST("S1 case 5: a bypassed effect (crashing or missing plugin) does not refuse; the same effect active does") {
  const fs::path dir = workDir("case5-bypass");
  const json effects = json::array({effect("fx1", "Core Test Crash", true)});
  const json s = song(json::array({track("synth", "Core Test Synth", 60, effects)}));
  // Bypassed + would crash if played: never played, export succeeds.
  {
    ScriptedProvider provider;
    provider.script["fx:synth:fx1"] = "crash";
    const fs::path target = dir / "bypass-crash.wav";
    core::RenderJobs jobs;
    const uint64_t id = jobs.start(request(s, target.string(), &provider))["jobId"].get<uint64_t>();
    const json st = waitForJob(jobs, id);
    CHECK_EQ(st.value("state", ""), std::string("succeeded"));
    CHECK(st.value("peak", 0.0) > 0.05);
    const core::RenderResult r = core::renderProject(core::fromJson(s), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0);
    CHECK(r.peak > 0.05f);
  }
  // Bypassed + plugin file missing: still intentional silence, export succeeds.
  {
    ScriptedProvider provider;
    provider.script["fx:synth:fx1"] = "missing";
    const fs::path target = dir / "bypass-missing.wav";
    core::RenderJobs jobs;
    const uint64_t id = jobs.start(request(s, target.string(), &provider))["jobId"].get<uint64_t>();
    const json st = waitForJob(jobs, id);
    CHECK_EQ(st.value("state", ""), std::string("succeeded"));
    const core::RenderResult r = core::renderProject(core::fromJson(s), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0);
    CHECK(r.peak > 0.05f);
  }
  // Control: the SAME effect NOT bypassed crashes mid-render -> refused, naming the effect's key.
  {
    const json active = song(json::array({track("synth", "Core Test Synth", 60, json::array({effect("fx1", "Core Test Crash", false)}))}));
    ScriptedProvider provider;
    provider.script["fx:synth:fx1"] = "crash";
    const fs::path target = dir / "active-crash.wav";
    seedWav(target, 0.3f);
    const std::string before = bytesOf(target);
    core::RenderJobs jobs;
    const uint64_t id = jobs.start(request(active, target.string(), &provider))["jobId"].get<uint64_t>();
    const json st = waitForJob(jobs, id);
    CHECK_EQ(st.value("state", ""), std::string("failed"));
    CHECK(names(st.value("error", ""), "fx:synth:fx1"));
    CHECK_EQ(bytesOf(target), before);
    const std::string msg = refusalOf([&] { core::renderProject(core::fromJson(active), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0); });
    CHECK(!msg.empty());
    CHECK(names(msg, "fx:synth:fx1"));
  }
  // The master chain too: a bypassed crashing master effect succeeds; an active one refuses the export.
  {
    ScriptedProvider provider;
    provider.script["fx:master:m1"] = "crash";
    const json bypassed = song(json::array({track("synth", "Core Test Synth", 60)}), json::array({effect("m1", "Core Test Crash", true)}));
    const core::RenderResult ok = core::renderProject(core::fromJson(bypassed), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0);
    CHECK(ok.peak > 0.05f);
    const json active = song(json::array({track("synth", "Core Test Synth", 60)}), json::array({effect("m1", "Core Test Crash", false)}));
    const std::string msg = refusalOf([&] { core::renderProject(core::fromJson(active), 48000, 512, false, 0.2, nullptr, nullptr, &provider, 0); });
    CHECK(!msg.empty());
    CHECK(names(msg, "fx:master:m1"));
  }
}

CORE_TEST_MAIN
