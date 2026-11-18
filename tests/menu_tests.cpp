// BUG-SWEEP-32 (2026-11-18): the plugin catalog menu ids overlapped the fixed action ids, so with a large
// catalog an action (native instrument / Open / Remove / On effect) could REPLACE the track instrument.
// Boundary sizes from the audit's menu-ranges.log: 500/501/600/601/620/621/640/641/1000/1001.
// The routing under test is app/desktop/src/MenuIds.h, extracted from MainComponent.cpp's two menu lambdas.
#include "support/check.hpp"
#include "MenuIds.h"
#include <string>
#include <vector>

namespace {
namespace menu = core::app::menu;

// The menus list doc::builtinInstruments() (8 entries today) and a chain capped at 16 (edit.hpp).
constexpr int kBuiltinCount = 8;
constexpr int kChainCount = 16;
constexpr int kFxCount = 300;  // the effect catalog is uncapped too; any size exercises the second band
const std::vector<int> kBoundarySizes = {500, 501, 600, 601, 620, 621, 640, 641, 1000, 1001, 5000};

const char* trackActionName(menu::TrackAction a) {
  switch (a) {
    case menu::TrackAction::none: return "none";
    case menu::TrackAction::rename: return "rename";
    case menu::TrackAction::duplicate: return "duplicate";
    case menu::TrackAction::removeTrack: return "removeTrack";
    case menu::TrackAction::automation: return "automation";
    case menu::TrackAction::openInstrument: return "openInstrument";
    case menu::TrackAction::replaceWithPlugin: return "replaceWithPlugin";
    case menu::TrackAction::addPluginEffect: return "addPluginEffect";
    case menu::TrackAction::openEffect: return "openEffect";
    case menu::TrackAction::removeEffect: return "removeEffect";
    case menu::TrackAction::toggleEffect: return "toggleEffect";
    case menu::TrackAction::replaceWithBuiltin: return "replaceWithBuiltin";
  }
  return "?";
}
const char* addTrackActionName(menu::AddTrackAction a) {
  switch (a) {
    case menu::AddTrackAction::none: return "none";
    case menu::AddTrackAction::addPluginInstrument: return "addPluginInstrument";
    case menu::AddTrackAction::addBuiltinInstrument: return "addBuiltinInstrument";
    case menu::AddTrackAction::samplerTrack: return "samplerTrack";
    case menu::AddTrackAction::audioTrack: return "audioTrack";
    case menu::AddTrackAction::importAudio: return "importAudio";
  }
  return "?";
}

void expectTrack(int catalog, int chosen, menu::TrackAction action, int index) {
  ++coretest::checks();
  const menu::TrackChoice got = menu::decodeTrackMenu(chosen, catalog, kFxCount, kBuiltinCount, kChainCount);
  if (got.action != action || got.index != index)
    coretest::fail(__FILE__, __LINE__,
                   "track menu catalog=" + std::to_string(catalog) + " chosen=" + std::to_string(chosen) +
                       ": measured " + trackActionName(got.action) + " index=" + std::to_string(got.index) +
                       ", expected " + trackActionName(action) + " index=" + std::to_string(index));
}
void expectAddTrack(int catalog, int chosen, menu::AddTrackAction action, int index) {
  ++coretest::checks();
  const menu::AddTrackChoice got = menu::decodeAddTrackMenu(chosen, catalog, kBuiltinCount);
  if (got.action != action || got.index != index)
    coretest::fail(__FILE__, __LINE__,
                   "add track menu catalog=" + std::to_string(catalog) + " chosen=" + std::to_string(chosen) +
                       ": measured " + addTrackActionName(got.action) + " index=" + std::to_string(got.index) +
                       ", expected " + addTrackActionName(action) + " index=" + std::to_string(index));
}

// The first fixed action id inside the plugin band [first, first + count), or -1 when the band is clean.
// Interval arithmetic: covers EVERY plugin index of the catalog, not a sample.
int firstCollision(int first, int count, const std::vector<int>& actionIds) {
  for (const int id : actionIds)
    if (id >= first && id - first < count) return id;
  return -1;
}
void expectNoCollision(int catalog, int bandFirst, int bandCount, const std::vector<int>& actionIds, const char* band) {
  ++coretest::checks();
  const int hit = firstCollision(bandFirst, bandCount, actionIds);
  if (hit >= 0)
    coretest::fail(__FILE__, __LINE__,
                   std::string(band) + " catalog=" + std::to_string(catalog) + ": plugin item id " + std::to_string(hit) +
                       " (index " + std::to_string(hit - bandFirst) + ") is also a fixed action id");
}

// Every fixed action id of the track menu: the small ones, the 8 builtin-replace ids, the whole 900..959
// chain block, and the disabled placeholder.
std::vector<int> trackActionIds() {
  std::vector<int> ids = {menu::kRename, menu::kDuplicate, menu::kRemoveTrack, menu::kAutomation, menu::kOpenInstrument, menu::kNoPlugins};
  for (int i = 0; i < kBuiltinCount; ++i) ids.push_back(menu::kBuiltinReplaceFirst + i);
  for (int id = menu::kEffectOpenFirst; id < menu::kEffectIdEnd; ++id) ids.push_back(id);
  return ids;
}
std::vector<int> addTrackActionIds() {
  std::vector<int> ids = {menu::kAudioTrack, menu::kImportAudio, menu::kSamplerTrack, menu::kNoPlugins};
  for (int i = 0; i < kBuiltinCount; ++i) ids.push_back(menu::kBuiltinAddFirst + i);
  return ids;
}
}  // namespace

TEST("track menu: action ids route to their action at every boundary catalog size") {
  for (const int n : kBoundarySizes) {
    expectTrack(n, 800, menu::TrackAction::replaceWithBuiltin, 0);  // native instrument, NOT replaceWithPlugin
    expectTrack(n, 807, menu::TrackAction::replaceWithBuiltin, 7);
    expectTrack(n, 900, menu::TrackAction::openEffect, 0);
    expectTrack(n, 920, menu::TrackAction::removeEffect, 0);
    expectTrack(n, 940, menu::TrackAction::toggleEffect, 0);
    expectTrack(n, 4000, menu::TrackAction::addPluginEffect, 0);
    expectTrack(n, 6, menu::TrackAction::openInstrument, -1);
    expectTrack(n, 1, menu::TrackAction::rename, -1);
    expectTrack(n, 2, menu::TrackAction::duplicate, -1);
    expectTrack(n, 3, menu::TrackAction::removeTrack, -1);
    expectTrack(n, 4, menu::TrackAction::automation, -1);
  }
}

TEST("track menu: plugin item ids never share an action id, at any boundary catalog size") {
  const std::vector<int> actions = trackActionIds();
  for (const int n : kBoundarySizes) {
    const menu::TrackMenuIds ids = menu::trackMenuIds(n, kFxCount);
    expectNoCollision(n, ids.replacePluginFirst, n, actions, "replace-instrument band");
    expectNoCollision(n, ids.addEffectPluginFirst, kFxCount, actions, "add-effect band");
    // The two uncapped bands must not overlap each other either.
    expectNoCollision(n, ids.addEffectPluginFirst, kFxCount, {ids.replacePluginFirst}, "add-effect band vs replace band start");
    ++coretest::checks();
    const bool bandsOverlap = ids.replacePluginFirst < ids.addEffectPluginFirst + kFxCount &&
                              ids.addEffectPluginFirst < ids.replacePluginFirst + n;
    if (bandsOverlap)
      coretest::fail(__FILE__, __LINE__, "track menu catalog=" + std::to_string(n) + ": the two plugin bands overlap");
    // Every plugin still round-trips to its own index (first and last of each list).
    expectTrack(n, ids.replacePluginFirst, menu::TrackAction::replaceWithPlugin, 0);
    expectTrack(n, ids.replacePluginFirst + n - 1, menu::TrackAction::replaceWithPlugin, n - 1);
    expectTrack(n, ids.addEffectPluginFirst, menu::TrackAction::addPluginEffect, 0);
    expectTrack(n, ids.addEffectPluginFirst + kFxCount - 1, menu::TrackAction::addPluginEffect, kFxCount - 1);
  }
}

TEST("track menu: the effect chain ids keep the cap-16 wrap semantics") {
  for (int i = 0; i < kChainCount; ++i) {
    expectTrack(10, 900 + i, menu::TrackAction::openEffect, i);
    expectTrack(10, 920 + i, menu::TrackAction::removeEffect, i);
    expectTrack(10, 940 + i, menu::TrackAction::toggleEffect, i);
  }
  // Past the chain (the % 20 wrap lands on 16..19): nothing happens, as in the callback.
  for (int id = 916; id < 920; ++id) expectTrack(10, id, menu::TrackAction::none, -1);
  for (int id = 936; id < 940; ++id) expectTrack(10, id, menu::TrackAction::none, -1);
  for (int id = 956; id < 960; ++id) expectTrack(10, id, menu::TrackAction::none, -1);
}

TEST("track menu: unknown ids do nothing") {
  for (const int id : {0, 5, 7, 960, 99999}) expectTrack(10, id, menu::TrackAction::none, -1);
}

TEST("add track menu: action ids route to their action at every boundary catalog size") {
  for (const int n : kBoundarySizes) {
    expectAddTrack(n, 3000, menu::AddTrackAction::addBuiltinInstrument, 0);  // native instrument, NOT addPluginInstrument
    expectAddTrack(n, 3007, menu::AddTrackAction::addBuiltinInstrument, 7);
    expectAddTrack(n, 1000, menu::AddTrackAction::audioTrack, -1);
    expectAddTrack(n, 1001, menu::AddTrackAction::importAudio, -1);
    expectAddTrack(n, 1002, menu::AddTrackAction::samplerTrack, -1);
  }
}

TEST("add track menu: plugin item ids never share an action id, at any boundary catalog size") {
  const std::vector<int> actions = addTrackActionIds();
  for (const int n : kBoundarySizes) {
    const int first = menu::addTrackPluginFirst();
    expectNoCollision(n, first, n, actions, "add-track plugin band");
    expectAddTrack(n, first, menu::AddTrackAction::addPluginInstrument, 0);
    expectAddTrack(n, first + n - 1, menu::AddTrackAction::addPluginInstrument, n - 1);
  }
}

TEST("add track menu: unknown ids do nothing") {
  for (const int id : {0, 999, 1003, 99999}) expectAddTrack(10, id, menu::AddTrackAction::none, -1);
}

CORE_TEST_MAIN