// Tests of the engine as the app runs it: the real process (core-engine serve) driven through the app's own client
// (app/engine-client), on the null audio backend. No window, no real audio device.
// Run: engine_process_tests [part of a name]
#include <algorithm>
#include <chrono>
#include <cstdio>
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

#ifdef CORE_TEST_CRASH_VST3
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
#endif

CORE_TEST_MAIN
