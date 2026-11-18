// Characterization of the legacy module cleanup (2026-11-18, PROD-02 / PROD-04 / PROD-06): the Tools cards
// keep their nine tool ids and functions, but the W-Core / F-Core / R-Core / D-Core module titles are gone
// (P-Core's card is the product name, Pattern Maker). The old ids and titles stay as LOOKUP ALIASES, so the
// AI's old names and a legacy project's identifiers still resolve. New shared buses get plain Reverb / Delay
// names; a legacy project's R-Core / D-Core bus is reused under its own name, never duplicated. The bridge's
// "open" takes the old module ids ("wcore" -> the track's instrument window, as its comment always promised).
// Run: modules_tests [part-of-name]
#include <string>
#include <vector>
#include "support/check.hpp"
#include "calls.hpp"
#include "core_plugins.hpp"
#include "document.hpp"
#include "edit.hpp"
#include "role_tools.hpp"
#include "roles.hpp"

using namespace core::doc;

namespace {
json fakePlugin(const std::string& name) {
  return {{"format", "vst3"}, {"id", "VST3-" + name + "-1234-abcd"}, {"name", name}, {"vendor", "Test"}, {"file", "C:/VST3/" + name + ".vst3"}};
}
RoleMap spaceMap() {
  RoleMap m;
  m.roles["reverb"] = guessRoleBinding("reverb", fakePlugin("ValhallaRoom"),
                                       json::array({{{"id", "0"}, {"name", "Mix"}}, {{"id", "2"}, {"name", "Decay"}}}));
  m.roles["delay"] = guessRoleBinding("delay", fakePlugin("ValhallaDelay"),
                                      json::array({{{"id", "3"}, {"name", "Dry/Wet"}}, {{"id", "4"}, {"name", "Feedback"}}}));
  return m;
}
std::vector<std::string> noteTracks(const Document& d) {
  std::vector<std::string> out;
  for (const auto& t : d.tracks) if (playsPatterns(t)) out.push_back(t.id);
  return out;
}
int busesNamed(const Document& d, const std::string& prefix) {
  int n = 0;
  for (const auto& t : d.tracks) n += t.instrument.kind == "bus" && t.name.rfind(prefix, 0) == 0;
  return n;
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
std::string canon(const Document& d) { return toJson(d).dump(); }

const std::vector<core::control::CoreFunction>& functions() {
  static const std::vector<core::control::CoreFunction> f = core::control::buildFunctions();
  return f;
}
json openProposal(const std::string& what) {
  return {{"id", "p1"}, {"summary", "open"}, {"calls", json::array({{{"fn", "open"}, {"on", "Kick"}, {"what", what}}})}};
}
}  // namespace

CORE_TEST_MAIN

TEST("module cleanup: the cards keep the nine tool ids, with no legacy *-Core title") {
  const std::vector<std::string> ids = {"pcore", "wcore", "fcore", "rcore", "dcore", "suck", "pum", "buildup", "roll"};
  CHECK_EQ(corePlugins().size(), ids.size());
  for (size_t i = 0; i < corePlugins().size() && i < ids.size(); ++i) {
    const CorePluginInfo& p = corePlugins()[i];
    CHECK_EQ(p.id, ids[i]);  // the ids stay: the AI and the menus call the same tools
    CHECK(!has(p.shortName, "Core"));  // the module titles left the cards (PROD-04)
    CHECK(!has(p.word, "Core"));
    CHECK(!has(p.about, "-Core"));
  }
  const auto problems = corePluginListProblems();
  for (const auto& s : problems) std::printf("  problem: %s\n", s.c_str());
  CHECK(problems.empty());
  // The Pattern Maker card carries the product name (PROD-02), not a second P-Core tool.
  CHECK_EQ(corePlugins()[0].shortName, std::string("Pattern Maker"));
}

TEST("module cleanup: the old module ids and titles still resolve as lookup aliases") {
  struct Alias { const char* name; const char* id; };
  const std::vector<Alias> aliases = {
      {"wcore", "wcore"}, {"W-Core", "wcore"}, {"w-core", "wcore"}, {"W-CORE", "wcore"}, {"synth", "wcore"},
      {"fcore", "fcore"}, {"F-Core", "fcore"}, {"f-core", "fcore"}, {"fx", "fcore"},
      {"rcore", "rcore"}, {"R-Core", "rcore"}, {"r-core", "rcore"}, {"space", "rcore"},
      {"dcore", "dcore"}, {"D-Core", "dcore"}, {"d-core", "dcore"}, {"echo", "dcore"},
      {"pcore", "pcore"}, {"P-Core", "pcore"}, {"p-core", "pcore"}, {"Pattern Maker", "pcore"}, {"patterns", "pcore"},
      {"pump", "pum"}, {"build up", "buildup"}, {"snare roll", "roll"}};
  for (const auto& a : aliases) {
    const CorePluginInfo* p = findCorePlugin(a.name);
    CHECK(p != nullptr);
    if (p) CHECK_EQ(p->id, std::string(a.id));
  }
  CHECK(findCorePlugin("score") == nullptr);  // S-Core is not a card: the score opens by openCoreWindow, kept
  CHECK(findCorePlugin("") == nullptr);
  CHECK(findCorePlugin("no such tool") == nullptr);
}

TEST("module cleanup: a new shared bus is named Reverb / Delay, and the sentence says so") {
  Document d = newTemplateDocument();
  const SharedBusResult r = toolSharedBus(d, noteTracks(d), spaceMap(), "reverb", "hall");
  CHECK(r.newBus);
  CHECK_EQ(trackById(d, r.busId).name, std::string("Reverb \xC2\xB7 Hall"));
  CHECK(has(r.sentence, "Reverb hall:"));
  CHECK(!has(r.sentence, "R-Core"));
  const SharedBusResult e = toolSharedBus(d, noteTracks(d), spaceMap(), "delay", "");  // "": dotted
  CHECK(e.newBus);
  CHECK_EQ(trackById(d, e.busId).name, std::string("Delay \xC2\xB7 Dotted"));
  CHECK(has(e.sentence, "Delay dotted:"));
  CHECK(!has(e.sentence, "D-Core"));
}

TEST("module cleanup: a legacy R-Core / D-Core bus is reused under its own name, not duplicated") {
  Document d = newTemplateDocument();
  const std::string reverbBus = addBusTrack(d, "R-Core \xC2\xB7 Hall");
  trackById(d, reverbBus).extra["corePlace"] = {{"kind", "reverb"}, {"place", "hall"}};
  const std::string delayBus = addBusTrack(d, "D-Core \xC2\xB7 Dotted");
  trackById(d, delayBus).extra["corePlace"] = {{"kind", "delay"}, {"place", "dotted"}};
  // A save/reopen keeps the old names and tags: they are the song's data, not a surface to rename.
  d = parseDocument(toJson(d));
  CHECK_EQ(trackById(d, reverbBus).name, std::string("R-Core \xC2\xB7 Hall"));
  CHECK_EQ(trackById(d, delayBus).name, std::string("D-Core \xC2\xB7 Dotted"));
  const SharedBusResult r = toolSharedBus(d, noteTracks(d), spaceMap(), "reverb", "hall");
  CHECK(!r.newBus);  // the place tag finds the old bus; no second bus under the new name
  CHECK_EQ(r.busId, reverbBus);
  CHECK_EQ(busesNamed(d, "R-Core"), 1);
  CHECK_EQ(busesNamed(d, "Reverb"), 0);
  CHECK_EQ(trackById(d, reverbBus).name, std::string("R-Core \xC2\xB7 Hall"));
  const SharedBusResult e = toolSharedBus(d, noteTracks(d), spaceMap(), "delay", "dotted");
  CHECK(!e.newBus);
  CHECK_EQ(e.busId, delayBus);
  CHECK_EQ(busesNamed(d, "D-Core"), 1);
  CHECK_EQ(busesNamed(d, "Delay"), 0);
  CHECK_EQ(canon(parseDocument(toJson(d))), canon(d));
}

TEST("module cleanup: a legacy project with old module identifiers opens and keeps its data") {
  json j = toJson(newTemplateDocument());
  json& bass = j["tracks"][1];
  bass["instrument"]["kind"] = "builtin";
  bass["instrument"]["name"] = "wave";  // W-Core's engine identifier stays readable (silent, kept as data)
  bass["instrument"]["version"] = 2;
  bass["instrument"]["synth"] = {{"layers", json::array({{{"lfos", json::array()}, {"links", json::array()}}})}};
  // An effect tagged by the old intent FX (the "fcore" mark): the tag stays, the effect stays.
  bass["effects"] = json::array({{{"id", "fx1"}, {"type", "builtin"}, {"name", "filter"}, {"fcore", true},
                                  {"params", {{"cutoff", 1200.0}}}}});
  j["tracks"].push_back({{"id", "rbus"}, {"name", "R-Core \xC2\xB7 Room"}, {"instrument", {{"kind", "bus"}}},
                         {"corePlace", {{"kind", "reverb"}, {"place", "room"}}}});
  const Document d = parseDocument(j);
  CHECK_EQ(d.tracks.size(), (size_t)4);
  CHECK_EQ(d.tracks[1].instrument.name, std::string("wave"));
  CHECK(d.tracks[1].instrument.synth.is_object());
  CHECK_EQ(d.tracks[1].effects[0].value("fcore", false), true);
  CHECK_EQ(d.tracks[3].name, std::string("R-Core \xC2\xB7 Room"));
  CHECK_EQ(d.tracks[3].extra.value("corePlace", json::object()).value("place", std::string()), std::string("room"));
  CHECK_EQ(canon(parseDocument(toJson(d))), canon(d));  // save/reopen byte for byte
  // The old bus is still a place bus: the tool reuses it instead of making a new one.
  Document e = d;
  const SharedBusResult r = toolSharedBus(e, noteTracks(e), spaceMap(), "reverb", "room");
  CHECK(!r.newBus);
  CHECK_EQ(r.busId, std::string("rbus"));
}

TEST("module cleanup: a new track of the old wave role is not named after the retired module") {
  Document d = newEmptyDocument();
  const std::string id = addTrack(d, "wave");
  CHECK_EQ(trackById(d, id).name, std::string("Synth"));  // the catalogue's own name, not "W-Core"
  CHECK_EQ(trackById(d, id).instrument.kind, std::string("none"));  // no built-in sound comes back
}

TEST("module cleanup: the bridge opens the old module ids (open aliases)") {
  const Document d = newTemplateDocument();
  auto openOf = [&](const std::string& what) {
    return core::control::validateProposal(openProposal(what), functions(), d, nullptr);
  };
  // The aliases that already worked keep working.
  for (const char* what : {"pcore", "stutter"}) {
    const auto p = openOf(what);
    CHECK_EQ(p.calls.size(), (size_t)1);
    if (p.calls.size() == 1) CHECK_EQ(p.calls[0].open, std::string("patterns"));
  }
  const auto stage = openOf("stage");
  CHECK_EQ(stage.calls.size(), (size_t)1);
  if (stage.calls.size() == 1) CHECK_EQ(stage.calls[0].open, std::string("score"));
  // "wcore" opens the track's instrument window, as calls.hpp's comment always promised (it did not).
  const auto w = openOf("wcore");
  CHECK_EQ(w.calls.size(), (size_t)1);
  if (w.calls.size() == 1) CHECK_EQ(w.calls[0].open, std::string("instrument"));
  // The tool-only ids have no window of their own any more: a clear refusal, not a silent no-op.
  for (const char* what : {"fcore", "rcore", "dcore"}) {
    const auto p = openOf(what);
    CHECK(p.calls.empty());
    CHECK(!p.problems.empty());
  }
}