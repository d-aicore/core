// Tests of the engine as the app runs it: the real process (core-engine serve) driven through the app's own client
// (app/engine-client), on the null audio backend. No window, no real audio device.
// Run: engine_process_tests [part of a name]
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include "support/check.hpp"
#include "engine_client.hpp"

using namespace std::chrono_literals;
using core::client::EngineClient;
using core::client::EngineError;
using core::client::EngineOptions;
using core::client::json;

namespace {

EngineOptions engineOptions() {
  EngineOptions o;
  o.binary = CORE_ENGINE_EXE;
  o.arguments = {"serve"};
  o.startTimeout = 20000ms;
  o.closeTimeout = 10000ms;  // an engine that does not exit by itself is terminated then, with exit code 1
  return o;
}

// The reason of the session's end ("The engine exited (code N).") as the client reports it.
struct EndReason {
  std::mutex mutex;
  std::string text;
  void attach(EngineClient& engine) {
    engine.setEventHandler([this](const json& e) {
      if (e.value("event", "") != "disconnected") return;
      std::lock_guard<std::mutex> g(mutex);
      text = e.value("reason", "");
    });
  }
  std::string get() { std::lock_guard<std::mutex> g(mutex); return text; }
};

// One command that must answer within the limit: the time it took (ms), or -1 with the reason printed.
double answersWithin(EngineClient& engine, const std::string& cmd, const json& args, std::chrono::milliseconds limit, json* result = nullptr) {
  const auto t0 = std::chrono::steady_clock::now();
  try {
    json r = engine.command(cmd, args, limit);
    if (result) *result = std::move(r);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "  %s: %s\n", cmd.c_str(), e.what());
    return -1;
  }
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

#if defined(CORE_TEST_CRASH_VST3) || defined(CORE_TEST_FAULT_VST3)
// The first plugin of a file as the engine describes it (core-engine scan): what a song's track carries.
json scanFirst(const std::string& file) {
  const std::string command = "\"\"" + std::string(CORE_ENGINE_EXE) + "\" scan \"" + file + "\"\"";
  FILE* pipe = _popen(command.c_str(), "r");
  if (!pipe) return json();
  std::string out;
  char buffer[4096];
  while (size_t n = std::fread(buffer, 1, sizeof buffer, pipe)) out.append(buffer, n);
  _pclose(pipe);
  const json found = json::parse(out, nullptr, false);
  return found.is_array() && !found.empty() ? found[0] : json();
}

json pluginTrack(const std::string& id, const json& plugin) {
  return {{"id", id}, {"name", id}, {"instrument", "plugin"}, {"plugin", plugin}, {"patternTicks", 3840},
          {"notes", json::array({{{"tick", 0}, {"length", 960}, {"pitch", 60}, {"velocity", 0.8}}})}};
}

json song(const json& tracks) {
  return {{"format", "core-project"}, {"version", 2}, {"name", "plugin crash"}, {"bpm", 120.0},
          {"lengthTicks", 64 * 960}, {"masterGainDb", 0.0}, {"tracks", tracks}};
}
#endif

}  // namespace

#ifdef CORE_TEST_CRASH_VST3
// M1 (2026-11-18): the host catches a plugin's crash with structured exception handling, so no destructor runs and
// JUCE's process lock stays held. Releasing or deleting that instance later (Retry, removing its track, another song)
// waited for the lock forever on the message thread, and from then on the engine answered nothing and did not exit.
TEST("serve: after a plugin crashes in processBlock the engine still answers, retries, releases it and exits") {
  const json crash = scanFirst(CORE_TEST_CRASH_VST3), synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(crash.is_object());
  CHECK(synth.is_object());
  if (!crash.is_object() || !synth.is_object()) return;

  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  const json loaded = engine.command("project.load", {{"project", song(json::array({pluginTrack("crash", crash), pluginTrack("synth", synth)}))}}, 30000ms);
  CHECK(loaded.value("missing", json::array()).empty());  // both test plugins loaded, or nothing is tested
  engine.command("engine.start", {{"backend", "null"}});
  engine.command("transport.play");

  // The plugin crashes 0.1 s into the song; the engine reports it and plays on.
  json failed = json::array();
  for (int i = 0; i < 100 && failed.empty(); ++i) {
    std::this_thread::sleep_for(100ms);
    failed = engine.command("plugins.failed");
  }
  CHECK_EQ(failed.size(), (size_t)1);
  if (failed.size() == 1) {
    CHECK_EQ(failed[0].value("key", std::string()), std::string("track:crash"));
    CHECK_EQ(failed[0].value("why", std::string()), std::string("crash"));
  }

  // Retry: a new instance, and the crashed one is let go. The engine goes on answering (each within 2 s) while and
  // after the old graph and its instance are dropped.
  json retry;
  CHECK(answersWithin(engine, "plugin.retry", {{"key", "track:crash"}}, 10000ms, &retry) >= 0);
  CHECK(retry.value("retried", false));
  CHECK(retry.value("loaded", false));
  double slowest = 0;
  bool answered = true;
  for (int i = 0; i < 10 && answered; ++i) {
    json states;
    const double a = answersWithin(engine, "plugins.states", json::object(), 2000ms, &states);
    const double b = a < 0 ? -1 : answersWithin(engine, "engine.stats", json::object(), 2000ms);
    answered = a >= 0 && b >= 0;
    if (answered) {
      slowest = std::max({slowest, a, b});
      CHECK(states.contains("track:synth"));  // the healthy plugin's state is still there to save
    }
    std::this_thread::sleep_for(150ms);
  }
  CHECK(answered);

  // The track is removed (the retried instance has crashed again by now), then Stop: still answering.
  CHECK(answersWithin(engine, "project.load", {{"project", song(json::array({pluginTrack("synth", synth)}))}}, 10000ms) >= 0);
  for (int i = 0; i < 10 && answered; ++i) {
    const double a = answersWithin(engine, "plugins.states", json::object(), 2000ms);
    answered = a >= 0;
    if (answered) slowest = std::max(slowest, a);
    std::this_thread::sleep_for(150ms);
  }
  CHECK(answered);
  CHECK(answersWithin(engine, "transport.stop", json::object(), 2000ms) >= 0);
  CHECK(answersWithin(engine, "engine.stats", json::object(), 2000ms) >= 0);
  std::printf("  slowest answer after the crash: %.0f ms\n", slowest);

  // Closing stdin ends the session: the engine exits by itself (code 0), it is not terminated.
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}

// Data loss (2026-11-18), S3: the host listened to parameter changes only. A plugin that changes what it holds
// without moving a parameter (a sample or a wavetable loaded in its window) tells the host through VST3's setDirty,
// which JUCE delivers as audioProcessorChanged: nobody listened, the app never heard "pluginsChanged", and the
// song closed without a question, losing the change.
TEST("serve: a plugin change that is not a parameter is reported as pluginsChanged") {
  const json synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(synth.is_object());
  if (!synth.is_object()) return;

  std::atomic<int> changes{0};
  EngineClient engine(engineOptions());
  engine.setEventHandler([&changes](const json& e) { if (e.value("event", "") == "pluginsChanged") ++changes; });
  engine.start();
  const json loaded = engine.command("project.load", {{"project", song(json::array({pluginTrack("synth", synth)}))}}, 30000ms);
  CHECK(loaded.value("missing", json::array()).empty());
  engine.command("engine.start", {{"backend", "null"}});
  std::this_thread::sleep_for(500ms);
  CHECK_EQ(changes.load(), 0);  // loading a plugin and giving it its state is not a change the user made

  // An ordinary note changes nothing; the test synth's note 0 reports a change of state that is not a parameter.
  engine.command("note.on", {{"trackId", "synth"}, {"pitch", 60}, {"velocity", 0.8}});
  engine.command("note.off", {{"trackId", "synth"}, {"pitch", 60}});
  std::this_thread::sleep_for(500ms);
  CHECK_EQ(changes.load(), 0);
  engine.command("note.on", {{"trackId", "synth"}, {"pitch", 0}, {"velocity", 0.8}});
  for (int i = 0; i < 50 && changes.load() == 0; ++i) std::this_thread::sleep_for(100ms);
  CHECK_EQ(changes.load(), 1);
  engine.command("note.off", {{"trackId", "synth"}, {"pitch", 0}});
  engine.close();
}
#endif

#ifdef CORE_TEST_FAULT_VST3
// A crash outside processBlock (2026-11-18, plugin-crash follow-ups): the host guards the state, reset and editor
// entry points with the same structured exception handling as processBlock, so a plugin that crashes in one of them
// is silenced and leaked, the engine answers the next command and the other tracks play on. These paths were already
// guarded; the tests are regression guards (measured green on the prior state), except plugin.params below, which
// was not guarded and took the engine down (its red is measured with the fix).
namespace {
// Sets an environment variable for the engine process (it inherits this one's) and clears it when the test ends.
struct EnvVar {
  std::string name;
  EnvVar(const char* n, const char* v) : name(n) { _putenv_s(n, v); }
  ~EnvVar() { _putenv_s(name.c_str(), ""); }
};

// True when the engine reports this key as a crashed plugin.
bool failedAsCrash(const json& failed, const std::string& key) {
  for (const auto& f : failed)
    if (f.value("key", std::string()) == key && f.value("why", std::string()) == "crash") return true;
  return false;
}

// The audio thread keeps running while the song plays: callbacks advance between two answers.
bool playingOn(EngineClient& engine) {
  json a, b;
  if (answersWithin(engine, "engine.stats", json::object(), 2000ms, &a) < 0) return false;
  std::this_thread::sleep_for(200ms);
  if (answersWithin(engine, "engine.stats", json::object(), 2000ms, &b) < 0) return false;
  return b.value("callbacks", (uint64_t)0) > a.value("callbacks", (uint64_t)0);
}
}  // namespace

TEST("serve: a plugin that crashes giving its state is silenced and leaked, the engine answers and plays on") {
  EnvVar fault("CORE_TEST_FAULT_GETSTATE", "1");
  const json faultPlugin = scanFirst(CORE_TEST_FAULT_VST3), synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(faultPlugin.is_object());
  CHECK(synth.is_object());
  if (!faultPlugin.is_object() || !synth.is_object()) return;

  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  const json loaded = engine.command("project.load", {{"project", song(json::array({pluginTrack("fault", faultPlugin), pluginTrack("synth", synth)}))}}, 30000ms);
  CHECK(loaded.value("missing", json::array()).empty());
  engine.command("engine.start", {{"backend", "null"}});
  engine.command("transport.play");
  std::this_thread::sleep_for(300ms);  // let it process: the getState fault fires only once it has played

  // The crash happens in getStateInformation: the host catches it, marks the plugin crashed and skips its state.
  json states;
  CHECK(answersWithin(engine, "plugins.states", json::object(), 5000ms, &states) >= 0);
  CHECK(states.contains("track:synth"));   // the healthy plugin's state is still captured
  CHECK(!states.contains("track:fault"));  // the crashed one is never asked again
  json failed;
  CHECK(answersWithin(engine, "plugins.failed", json::object(), 2000ms, &failed) >= 0);
  CHECK(failedAsCrash(failed, "track:fault"));
  CHECK(playingOn(engine));  // the song plays on without the crashed plugin

  // Retry: a new instance from the document's state; the engine keeps answering and exits by itself.
  json retry;
  CHECK(answersWithin(engine, "plugin.retry", {{"key", "track:fault"}}, 10000ms, &retry) >= 0);
  CHECK(retry.value("retried", false));
  CHECK(retry.value("loaded", false));
  CHECK(answersWithin(engine, "engine.stats", json::object(), 2000ms) >= 0);
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}

TEST("serve: a plugin that crashes loading its state is reported missing, the engine answers and plays on") {
  const json faultPlugin = scanFirst(CORE_TEST_FAULT_VST3), synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(faultPlugin.is_object());
  CHECK(synth.is_object());
  if (!faultPlugin.is_object() || !synth.is_object()) return;

  // A healthy instance first, to capture a real state blob (JUCE's VST3 state format: a raw chunk is ignored).
  std::string stateBlob;
  {
    EngineClient healthy(engineOptions());
    healthy.start();
    healthy.command("project.load", {{"project", song(json::array({pluginTrack("fault", faultPlugin)}))}}, 30000ms);
    const json states = healthy.command("plugins.states");
    stateBlob = states.value("track:fault", std::string());
    healthy.close();
  }
  CHECK(!stateBlob.empty());
  if (stateBlob.empty()) return;

  // With the setState fault, loading that state crashes inside create(): the host catches it and reports why.
  EnvVar fault("CORE_TEST_FAULT_SETSTATE", "1");
  json faultWithState = faultPlugin;
  faultWithState["state"] = stateBlob;
  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  json loaded;
  CHECK(answersWithin(engine, "project.load", {{"project", song(json::array({pluginTrack("fault", faultWithState), pluginTrack("synth", synth)}))}}, 30000ms, &loaded) >= 0);
  bool named = false;
  for (const auto& m : loaded.value("missing", json::array()))
    if (m.value("key", std::string()) == "track:fault" && m.value("why", std::string()).find("crashed while it was loaded") != std::string::npos) named = true;
  CHECK(named);
  engine.command("engine.start", {{"backend", "null"}});
  engine.command("transport.play");
  CHECK(playingOn(engine));  // the synth plays; the fault track is silent
  json states;
  CHECK(answersWithin(engine, "plugins.states", json::object(), 2000ms, &states) >= 0);
  CHECK(states.contains("track:synth"));
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}

TEST("serve: a plugin that crashes in reset is silenced, the engine answers and plays on") {
  EnvVar fault("CORE_TEST_FAULT_RESET", "1");
  const json faultPlugin = scanFirst(CORE_TEST_FAULT_VST3), synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(faultPlugin.is_object());
  CHECK(synth.is_object());
  if (!faultPlugin.is_object() || !synth.is_object()) return;

  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  const json loaded = engine.command("project.load", {{"project", song(json::array({pluginTrack("fault", faultPlugin), pluginTrack("synth", synth)}))}}, 30000ms);
  CHECK(loaded.value("missing", json::array()).empty());
  engine.command("engine.start", {{"backend", "null"}});
  engine.command("transport.play");
  CHECK(playingOn(engine));

  // Stop resets every plugin (drops its tails): the crash in reset() is caught and the plugin silenced.
  CHECK(answersWithin(engine, "transport.stop", json::object(), 5000ms) >= 0);
  json failed;
  CHECK(answersWithin(engine, "plugins.failed", json::object(), 2000ms, &failed) >= 0);
  CHECK(failedAsCrash(failed, "track:fault"));
  CHECK(answersWithin(engine, "engine.stats", json::object(), 2000ms) >= 0);
  // Play again: the healthy synth still sounds, the crashed one stays silent.
  engine.command("transport.play");
  CHECK(playingOn(engine));
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}

TEST("serve: a plugin that crashes opening its editor is silenced, the engine answers and plays on") {
  EnvVar fault("CORE_TEST_FAULT_EDITOR", "1");
  const json faultPlugin = scanFirst(CORE_TEST_FAULT_VST3), synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(faultPlugin.is_object());
  CHECK(synth.is_object());
  if (!faultPlugin.is_object() || !synth.is_object()) return;

  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  const json loaded = engine.command("project.load", {{"project", song(json::array({pluginTrack("fault", faultPlugin), pluginTrack("synth", synth)}))}}, 30000ms);
  CHECK(loaded.value("missing", json::array()).empty());
  engine.command("engine.start", {{"backend", "null"}});
  engine.command("transport.play");

  // The crash happens in createEditor: the host catches it, marks the plugin crashed and refuses the window.
  bool refused = false;
  std::string why;
  try { engine.command("plugin.editor", {{"key", "track:fault"}, {"open", true}}, 5000ms); }
  catch (const EngineError& e) { refused = true; why = e.what(); }
  CHECK(refused);
  CHECK(why.find("crashed while it opened its window") != std::string::npos);
  json failed;
  CHECK(answersWithin(engine, "plugins.failed", json::object(), 2000ms, &failed) >= 0);
  CHECK(failedAsCrash(failed, "track:fault"));
  CHECK(playingOn(engine));
  // The healthy plugin's window still opens and closes (the editor path is not broken for everyone).
  CHECK(answersWithin(engine, "plugin.editor", {{"key", "track:synth"}, {"open", true}}, 5000ms) >= 0);
  CHECK(answersWithin(engine, "plugin.editor", {{"key", "track:synth"}, {"open", false}}, 5000ms) >= 0);
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}
#endif

// M2 (2026-11-18): one protocol line carries the state of every plugin of the song. The limit was 8 MB and 27 tracks
// of Vital with its default patch (318 128 bytes of base64 each) pass it: the client refused to send the song, and
// ended the engine when an answer (plugins.states on Save or Export) was that large.
namespace {
// A song of plugin tracks that carry a state each. The plugins have no file, so they are not loaded (the tracks are
// silent): what is tested is the song and its states going through the pipes, both ways.
json stateSong(int tracks, size_t stateBytes) {
  json list = json::array();
  for (int i = 0; i < tracks; ++i) {
    std::string state(stateBytes, 'A');
    state[0] = (char)('B' + i % 20);  // each track its own
    list.push_back({{"id", "t" + std::to_string(i)}, {"name", "Synth"}, {"instrument", "plugin"},
                    {"plugin", {{"format", "vst3"}, {"id", "not-installed"}, {"state", std::move(state)}}},
                    {"patternTicks", 3840}, {"notes", json::array()}});
  }
  return {{"format", "core-project"}, {"version", 2}, {"name", "states"}, {"bpm", 120.0},
          {"lengthTicks", 64 * 960}, {"masterGainDb", 0.0}, {"tracks", list}};
}

bool sameStates(const json& sent, const json& got) {
  if (!got.is_object() || !got.contains("tracks") || got["tracks"].size() != sent["tracks"].size()) return false;
  for (size_t i = 0; i < sent["tracks"].size(); ++i)
    if (got["tracks"][i].value("plugin", json::object()).value("state", std::string()) != sent["tracks"][i]["plugin"]["state"].get<std::string>()) return false;
  return true;
}
}  // namespace

TEST("serve: a song with more than 8 MB of plugin state reaches the engine and comes back whole (M2)") {
  const json song = stateSong(27, 318128);
  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  CHECK(engine.maxMessageBytes() >= (size_t)(64u << 20));  // the engine's limit, from its hello
  CHECK_EQ(engine.maxMessageBytes(), engine.hello()["limits"]["maxMessageBytes"].get<size_t>());

  json loaded, got;
  const double in = answersWithin(engine, "project.load", {{"project", song}}, 30000ms, &loaded);  // 8.6 MB to the engine
  CHECK(in >= 0);
  if (in < 0) return;  // not sent or not answered: nothing below can be read
  CHECK_EQ(loaded.value("tracks", (size_t)0), (size_t)27);
  const double out = answersWithin(engine, "project.get", json::object(), 30000ms, &got);         // and 8.6 MB back
  CHECK(out >= 0);
  CHECK(got.dump().size() > (size_t)(8u << 20));
  CHECK(sameStates(song, got));
  CHECK(engine.running());
  CHECK(answersWithin(engine, "engine.stats", json::object(), 2000ms) >= 0);
  std::printf("  project.load %.0f ms, project.get %.0f ms\n", in, out);
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}

TEST("serve: a message over the limit fails that command with a clear error and the engine stays alive (M2)") {
  const json song = stateSong(4, 500000);  // 2 MB
  EndReason end;
  EngineClient engine(engineOptions());
  end.attach(engine);
  engine.start();
  const size_t limit = engine.maxMessageBytes();
  engine.command("project.load", {{"project", song}}, 30000ms);
  engine.setMaxMessageBytesForTest(1u << 20);

  // An answer over the limit: that command fails, saying why; the session is not ended.
  bool protocol = false;
  std::string why;
  try { engine.command("project.get", json::object(), 10000ms); }
  catch (const EngineError& e) { protocol = e.kind == EngineError::Kind::Protocol; why = e.what(); }
  CHECK(protocol);
  CHECK(why.find("too large") != std::string::npos);
  CHECK(engine.running());
  CHECK(answersWithin(engine, "engine.stats", json::object(), 2000ms) >= 0);  // the next answer is read from its own line

  // A command over the limit: not sent, the same kind of error, the engine untouched.
  protocol = false;
  try { engine.command("project.load", {{"project", song}}, 10000ms); }
  catch (const EngineError& e) { protocol = e.kind == EngineError::Kind::Protocol; }
  CHECK(protocol);
  CHECK(engine.running());

  // With the engine's own limit again the song it holds is still there, whole.
  engine.setMaxMessageBytesForTest(limit);
  json got;
  CHECK(answersWithin(engine, "project.get", json::object(), 10000ms, &got) >= 0);
  CHECK(sameStates(song, got));
  engine.close();
  CHECK_EQ(end.get(), std::string("The engine exited (code 0)."));
}

CORE_TEST_MAIN
