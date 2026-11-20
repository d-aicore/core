// S2+S3 (DATA priority, owner round-2 item 5): a song whose plugins cannot load, or whose plugin state is
// invalid, must be ANNOUNCED and must stay RECOVERABLE — the document's original state is never replaced by
// factory defaults in silence. Driven as the app runs it: the real engine process (core-engine serve) through
// the app's own client (app/engine-client) on the null audio backend, plus the app's own EngineSession (the
// snapshot the window reads) and the app's own document save/reopen path. No window, no real audio device.
// Red measured at a9a0384 + round-1 fixes (coordinator-round2/s2s3-notes.md):
//   S2: the load answer carries missing {key, plugin, why} but the session never announces it (message stays
//       the device line); relinkPlugins has zero callers, so a stale reference stays missing after a rescan.
//   S3: an invalid state blob is ignored (factory defaults, no why); plugins.states then hands the factory
//       blob to applyPluginStates, which overwrites the document's original — save/reopen persists the loss.
// Run: state_recovery_tests [part of a name]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include "support/check.hpp"
#include "engine_client.hpp"
#include "document.hpp"
#include "edit.hpp"
#include "storage.hpp"
#include "plugin_refs.hpp"
#include "EngineSession.h"
#include <juce_events/juce_events.h>

using namespace std::chrono_literals;
using core::client::EngineClient;
using core::client::EngineError;
using core::client::EngineOptions;
using core::client::json;
namespace doc = core::doc;

namespace {

EngineOptions engineOptions() {
  EngineOptions o;
  o.binary = CORE_ENGINE_EXE;
  o.arguments = {"serve"};
  o.startTimeout = 20000ms;
  o.closeTimeout = 10000ms;
  return o;
}

// Waits for the condition while the JUCE message loop runs (EngineSession's async answers ride it).
template <class F> bool waitUntil(F cond, int timeoutMs) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    if (cond()) return true;
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
  }
  return cond();
}

std::filesystem::path workDir() {
  const auto dir = std::filesystem::temp_directory_path() / "core-s2s3-state-recovery";
  std::filesystem::create_directories(dir);
  return dir;
}

// The plugin of a song that points at a file this computer does not have (another PC, a moved folder).
json missingPluginRef() {
  return {{"format", "VST3"}, {"id", "VST3-No Such Plugin-0badf00d-12345678"}, {"name", "No Such Plugin"},
          {"vendor", "Core tests"}, {"file", "Z:/core-s2s3-tests/nonexistent/No Such Plugin.vst3"}};
}

// The state blob of a document's plugin place (the document is the owner of the original).
std::string stateOf(const doc::Document& d, const std::string& trackId) {
  return doc::trackById(const_cast<doc::Document&>(d), trackId).instrument.plugin.value("state", std::string());
}

// project.load as the app sends it (STATE-PER-EDIT): the projection names the states (hasState), the engine
// answers statesNeeded, the document supplies exactly those blobs, the second pass is the accepted one.
json loadAsApp(EngineClient& engine, const doc::Document& d) {
  const json projection = doc::toProjection(d);
  json result = engine.command("project.load", {{"project", projection}}, 30000ms);
  const json needed = result.value("statesNeeded", json::array());
  if (!needed.empty()) {
    std::vector<std::string> keys;
    for (const auto& k : needed) if (k.is_string()) keys.push_back(k.get<std::string>());
    const json supply = doc::pluginStatesFor(d, keys);
    result = engine.command("project.load", {{"project", projection}, {"states", supply}}, 30000ms);
  }
  return result;
}

// The missing entry of a load answer for one key ({} when the engine loaded it).
json missingEntry(const json& loadResult, const std::string& key) {
  for (const auto& m : loadResult.value("missing", json::array()))
    if (m.is_object() && m.value("key", std::string()) == key) return m;
  return json::object();
}

#if defined(CORE_TEST_SYNTH_VST3)
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

// A stale reference to a plugin this computer HAS: same format/name/uid, another machine's path and path hash.
json staleRef(const json& scanned, const std::string& state) {
  const doc::PluginKey key = doc::pluginKey(scanned);
  json stale = scanned;
  stale["file"] = "Z:/core-s2s3-tests/other-pc/" + scanned.value("name", std::string("plugin")) + ".vst3";
  stale["id"] = "VST3-" + scanned.value("name", std::string()) + "-0badf00d-" + key.uid;
  if (!state.empty()) stale["state"] = state;
  return stale;
}
#endif

}  // namespace

// ---- S2 (a): opening an affected project must say which plugin and why -------------------------------------

TEST("S2: a missing plugin is named with its why in the load answer AND announced by the session") {
  // The engine answer: the data is there (key, plugin, why).
  {
    EngineClient engine(engineOptions());
    engine.start();
    doc::Document d = doc::newEmptyDocument();
    const std::string trackId = doc::addPluginTrack(d, missingPluginRef());
    const json loaded = loadAsApp(engine, d);
    CHECK(loaded.value("statesNeeded", json::array()).empty());  // accepted
    const json m = missingEntry(loaded, doc::pluginKey(trackId));
    CHECK(m.contains("plugin"));
    CHECK_EQ(m.value("plugin", std::string()), std::string("No Such Plugin"));
    CHECK(!m.value("why", std::string()).empty());
    engine.close();
  }
  // The app session (what the window reads): the same song opened on a running engine must carry a notice
  // naming the plugin and the why — not only the disabled Open menu (MainComponent.cpp:4687/4698/5300).
  {
    core::app::EngineSession::Options o;
    o.engineBinary = CORE_ENGINE_EXE;
    o.backend = "null";
    core::app::EngineSession session(o, doc::newEmptyDocument());
    CHECK(waitUntil([&] { const auto s = session.snapshot(); return s.status == core::app::EngineStatus::Ready; }, 20000));
    doc::Document d = doc::newEmptyDocument();
    const std::string trackId = doc::addPluginTrack(d, missingPluginRef());
    session.setDocument(d);  // the open flow: another document on the running engine
    const std::string key = doc::pluginKey(trackId);
    CHECK(waitUntil([&] { return session.snapshot().missingPlugins.count(key) > 0; }, 20000));
    const auto snap = session.snapshot();
    CHECK_EQ(snap.missingPlugins.at(key).substr(0, 14), std::string("No Such Plugin"));  // "name: why" stored
    // The notice the user sees (message_ shows snap.message, MainComponent.cpp:6726):
    CHECK(snap.message.find("No Such Plugin") != std::string::npos);
    CHECK(snap.message.find("not found") != std::string::npos);
  }
}

// ---- S2 (b): relink — the function's contract, and the recovery it provides --------------------------------

TEST("S2: relink identifies the instance by format+name+uid, preserves state and automation") {
  doc::Document d = doc::newEmptyDocument();
  json otherPc = {{"format", "VST3"}, {"id", "VST3-Core Test Synth-0badf00d-12345678"}, {"name", "Core Test Synth"},
                  {"file", "Z:/other-pc/Core Test Synth.vst3"}};
  json withA = otherPc; withA["state"] = "c3RhdGUtYQ==";  // distinct state per instance
  json withB = otherPc; withB["state"] = "c3RhdGUtYg==";
  const std::string t1 = doc::addPluginTrack(d, withA);
  const std::string t2 = doc::addPluginTrack(d, withB);
  doc::setAutomationPoint(d, t1, "gainDb", 0, -6.0);
  doc::setAutomationPoint(d, t1, "gainDb", 960, 0.0);
  doc::setAutomationPoint(d, t2, "pan", 480, 0.5);
  json fx = {{"format", "VST3"}, {"id", "VST3-Core Test Gain-0badf00d-87654321"}, {"name", "Core Test Gain"},
             {"file", "Z:/other-pc/Core Test Gain.vst3"}, {"state", "c3RhdGUtYw=="}};
  doc::addPluginEffect(d, "", fx);  // master chain
  const json automationBefore = doc::toJson(d)["tracks"][0]["automation"];
  const json masterAutomationBefore = doc::toJson(d).value("masterAutomation", json::array());

  // This computer's scan: the same plugins, another path (and path hash in the id).
  const json localSynth = {{"format", "VST3"}, {"id", "VST3-Core Test Synth-11111111-12345678"}, {"name", "Core Test Synth"},
                           {"file", "C:/plugins/Core Test Synth.vst3"}};
  const json localGain = {{"format", "VST3"}, {"id", "VST3-Core Test Gain-22222222-87654321"}, {"name", "Core Test Gain"},
                          {"file", "C:/plugins/Core Test Gain.vst3"}};
  const int n = doc::relinkPlugins(d, {localSynth, localGain});
  CHECK_EQ(n, 3);  // two track instances + the master effect, each identified unambiguously
  CHECK_EQ(doc::trackById(d, t1).instrument.plugin["id"], localSynth["id"]);
  CHECK_EQ(doc::trackById(d, t1).instrument.plugin["file"], localSynth["file"]);
  CHECK_EQ(doc::trackById(d, t2).instrument.plugin["id"], localSynth["id"]);
  CHECK_EQ(stateOf(d, t1), std::string("c3RhdGUtYQ=="));  // each instance keeps its OWN state
  CHECK_EQ(stateOf(d, t2), std::string("c3RhdGUtYg=="));
  CHECK_EQ(doc::toJson(d)["tracks"][0]["automation"].dump(), automationBefore.dump());  // automation untouched
  CHECK_EQ(doc::toJson(d).value("masterAutomation", json::array()).dump(), masterAutomationBefore.dump());
  CHECK_EQ(d.masterEffects[0]["plugin"]["id"], localGain["id"]);
  CHECK_EQ(d.masterEffects[0]["plugin"]["state"].get<std::string>(), std::string("c3RhdGUtYw=="));

  // Already this computer's plugin: nothing changes.
  CHECK_EQ(doc::relinkPlugins(d, {localSynth, localGain}), 0);
}

TEST("S2: relink with no match or two matches leaves the project recoverable (never a guess)") {
  doc::Document d = doc::newEmptyDocument();
  json otherPc = {{"format", "VST3"}, {"id", "VST3-Core Test Synth-0badf00d-12345678"}, {"name", "Core Test Synth"},
                  {"file", "Z:/other-pc/Core Test Synth.vst3"}, {"state", "c3RhdGUtYQ=="}};
  const std::string t1 = doc::addPluginTrack(d, otherPc);
  const std::string before = doc::toJson(d).dump();

  CHECK_EQ(doc::relinkPlugins(d, {}), 0);  // no scan yet
  CHECK_EQ(doc::toJson(d).dump(), before);
  const json wrongUid = {{"format", "VST3"}, {"id", "VST3-Core Test Synth-11111111-99999999"}, {"name", "Core Test Synth"},
                         {"file", "C:/plugins/Core Test Synth.vst3"}};
  CHECK_EQ(doc::relinkPlugins(d, {wrongUid}), 0);  // another plugin with the same name: never taken
  CHECK_EQ(doc::toJson(d).dump(), before);
  const json copyA = {{"format", "VST3"}, {"id", "VST3-Core Test Synth-11111111-12345678"}, {"name", "Core Test Synth"},
                      {"file", "C:/plugins/a/Core Test Synth.vst3"}};
  const json copyB = {{"format", "VST3"}, {"id", "VST3-Core Test Synth-22222222-12345678"}, {"name", "Core Test Synth"},
                      {"file", "C:/plugins/b/Core Test Synth.vst3"}};
  CHECK_EQ(doc::relinkPlugins(d, {copyA, copyB}), 0);  // two copies: the user picks, the song stays as it was
  CHECK_EQ(doc::toJson(d).dump(), before);
  CHECK_EQ(stateOf(d, t1), std::string("c3RhdGUtYQ=="));  // the original state is still there to recover with
}

#if defined(CORE_TEST_SYNTH_VST3)
TEST("S2: a stale reference stays missing without relink; relink recovers it with its state") {
  const json synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(synth.is_object());
  if (!synth.is_object()) return;
  EngineClient engine(engineOptions());
  engine.start();

  // A healthy load first: the state this plugin's sound is made of (the original to preserve).
  doc::Document healthy = doc::newEmptyDocument();
  const std::string healthyTrack = doc::addPluginTrack(healthy, synth);
  const std::string healthyKey = doc::pluginKey(healthyTrack);
  json loaded = loadAsApp(engine, healthy);
  CHECK(missingEntry(loaded, healthyKey).empty());
  const json states0 = engine.command("plugins.states", {{"keys", json::array({healthyKey})}});
  CHECK(states0.contains(healthyKey));
  const std::string original = states0.value(healthyKey, std::string());
  CHECK(!original.empty());

  // The same plugin as another computer's reference: the engine cannot load it and says why.
  doc::Document d = doc::newEmptyDocument();
  const std::string trackId = doc::addPluginTrack(d, staleRef(synth, original));
  const std::string key = doc::pluginKey(trackId);
  loaded = loadAsApp(engine, d);
  json m = missingEntry(loaded, key);
  CHECK(!m.empty());
  CHECK(!m.value("why", std::string()).empty());
  // A rescan changes nothing by itself: loading the same document again is still missing (the engine has no
  // relink path; at a9a0384 relinkPlugins had ZERO callers — git grep, s2s3-notes.md). This is the red of (b):
  // the plugin is available again, yet the reference stays missing until the app relinks it.
  loaded = loadAsApp(engine, d);
  m = missingEntry(loaded, key);
  CHECK(!m.empty());

  // The recovery the app must wire: relink against the scan, then the same document loads with its state.
  CHECK_EQ(doc::relinkPlugins(d, {synth}), 1);
  CHECK_EQ(doc::trackById(d, trackId).instrument.plugin["id"], synth["id"]);
  CHECK_EQ(doc::trackById(d, trackId).instrument.plugin["file"], synth["file"]);
  CHECK_EQ(stateOf(d, trackId), original);  // relink never touches the state
  loaded = loadAsApp(engine, d);
  CHECK(missingEntry(loaded, key).empty());
  const json states1 = engine.command("plugins.states", {{"keys", json::array({key})}});
  CHECK_EQ(states1.value(key, std::string()), original);  // the recovered instance holds the original state
  engine.close();
}

// ---- S3 (c): invalid state — reported, and the original preserved across save/reopen -----------------------

// The app's save path with an invalid state blob: the engine's answer (plugins.states) is adopted into the
// document (MainComponent::fetchPluginStates -> adoptPluginStates -> doc::applyPluginStates) and saved
// (writeTo -> doc::saveFile). The original blob must survive all of it, and the instance must be identified.
void invalidStateCase(const std::string& name, const std::string& badState) {
  const json synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(synth.is_object());
  if (!synth.is_object()) return;
  json plugin = synth;
  plugin["state"] = badState;
  doc::Document d = doc::newEmptyDocument();
  const std::string trackId = doc::addPluginTrack(d, plugin);
  const std::string key = doc::pluginKey(trackId);
  const std::string original = stateOf(d, trackId);
  CHECK_EQ(original, badState);

  EngineClient engine(engineOptions());
  engine.start();
  const json loaded = loadAsApp(engine, d);
  CHECK(loaded.value("statesNeeded", json::array()).empty());  // accepted: the handshake checks presence, not validity
  // The instance is identified: missing, with a why that says the state is invalid (never factory defaults
  // in silence, never a loaded plugin whose state was ignored).
  const json m = missingEntry(loaded, key);
  CHECK(!m.empty());
  CHECK_EQ(m.value("plugin", std::string()), synth.value("name", std::string()));
  CHECK(m.value("why", std::string()).find("invalid state") != std::string::npos);

  // The live-state refresh (plugins.states, what Save adopts): a failed-state instance must not hand over
  // factory state — the document keeps the original for recovery.
  const json states = engine.command("plugins.states", {{"keys", json::array({key})}});
  CHECK(!states.contains(key));

  // The app's adoption + save/reopen (the data-loss path): the original blob survives byte-for-byte.
  doc::Document adopted = d;
  std::vector<std::string> changed;
  doc::applyPluginStates(adopted, states, &changed);
  CHECK(changed.empty());
  CHECK_EQ(stateOf(adopted, trackId), original);
  const auto file = workDir() / (name + ".core");
  doc::saveFile(adopted, file);
  const doc::OpenResult reopened = doc::openFile(file);
  CHECK_EQ(stateOf(reopened.document, reopened.document.tracks[0].id), original);
  engine.close();
}

TEST("S3: invalid base64 state is reported and the original survives save/reopen") {
  invalidStateCase("s3-invalid-base64", "%%% not base64 %%%");
}

TEST("S3: valid-base64 garbage state is reported and the original survives save/reopen") {
  // 64 bytes that are not a VST3 state blob (JUCE binary XML): the wrapper would ignore them in silence.
  std::string bytes;
  for (int i = 0; i < 64; ++i) bytes.push_back((char)(i * 7 + 3));
  const std::string garbage = juce::Base64::toBase64(bytes.data(), bytes.size()).toStdString();
  invalidStateCase("s3-garbage-blob", garbage);
}

TEST("S3 control: a healthy state loads bit-identical and saves unchanged") {
  const json synth = scanFirst(CORE_TEST_SYNTH_VST3);
  CHECK(synth.is_object());
  if (!synth.is_object()) return;
  EngineClient engine(engineOptions());
  engine.start();

  // The factory state of a healthy load: a valid blob to reopen with.
  doc::Document first = doc::newEmptyDocument();
  const std::string firstTrack = doc::addPluginTrack(first, synth);
  const std::string firstKey = doc::pluginKey(firstTrack);
  json loaded = loadAsApp(engine, first);
  CHECK(missingEntry(loaded, firstKey).empty());
  const json states0 = engine.command("plugins.states", {{"keys", json::array({firstKey})}});
  const std::string healthy = states0.value(firstKey, std::string());
  CHECK(!healthy.empty());

  // Reopen with that state: loaded (no missing), and the state round-trips bit-identical.
  json plugin = synth;
  plugin["state"] = healthy;
  doc::Document d = doc::newEmptyDocument();
  const std::string trackId = doc::addPluginTrack(d, plugin);
  const std::string key = doc::pluginKey(trackId);
  loaded = loadAsApp(engine, d);
  CHECK(loaded.value("statesNeeded", json::array()).empty());
  CHECK(missingEntry(loaded, key).empty());
  const json states1 = engine.command("plugins.states", {{"keys", json::array({key})}});
  CHECK_EQ(states1.value(key, std::string()), healthy);

  // Adoption changes nothing; save/reopen keeps the blob byte-for-byte.
  doc::Document adopted = d;
  std::vector<std::string> changed;
  doc::applyPluginStates(adopted, states1, &changed);
  CHECK(changed.empty());
  const auto file = workDir() / "s3-healthy-control.core";
  doc::saveFile(adopted, file);
  const doc::OpenResult reopened = doc::openFile(file);
  CHECK_EQ(stateOf(reopened.document, reopened.document.tracks[0].id), healthy);
  engine.close();
}
#endif  // CORE_TEST_SYNTH_VST3

int main(int argc, char** argv) {
  juce::MessageManager::getInstance();  // EngineSession's async answers ride the message loop
  const int rc = coretest::runAll(argc, argv);
  juce::MessageManager::deleteInstance();
  return rc;
}
