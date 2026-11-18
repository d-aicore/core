// The identifier allocation and the dispatch decision of the two plugin-catalog menus (BUG-SWEEP-32).
// Extracted from MainComponent.cpp (showAddTrackMenu / showTrackMenu) so the routing is testable headless:
// the menus themselves are inline lambdas over juce::PopupMenu. Pure header, no JUCE, no document state.
//
// The catalog is NOT capped: Plugins.cpp returns every instrument / effect it found. Disjointness by
// construction (BUG-SWEEP-32): every fixed action id is below kPluginBandFirst (or the negative placeholder),
// and the plugin bands start at kPluginBandFirst and stack upward, sized by the catalogs:
//   track menu:     add-effect [4000, 4000 + effectCount), replace [4000 + effectCount, 4000 + effectCount + instrumentCount)
//   add-track menu: plugin instruments [4000, 4000 + instrumentCount)
// So no plugin index can share an action id at any catalog size, and the two track bands never overlap.
#pragma once

namespace core::app::menu {

// Fixed action ids of the track context menu (showTrackMenu).
inline constexpr int kRename = 1;
inline constexpr int kDuplicate = 2;
inline constexpr int kRemoveTrack = 3;
inline constexpr int kAutomation = 4;
inline constexpr int kOpenInstrument = 6;          // the track instrument's own window
inline constexpr int kEffectOpenFirst = 900;       // + place in the track's effect chain
inline constexpr int kEffectRemoveFirst = 920;     // + place in the chain
inline constexpr int kEffectOnFirst = 940;         // + place in the chain
inline constexpr int kEffectIdSpan = 20;           // the chain ids wrap % 20; the chain cap is 16 (edit.hpp)
inline constexpr int kEffectIdEnd = 960;           // exclusive end of the 900..959 chain block
inline constexpr int kNoPlugins = -1;              // the disabled "None found yet" placeholder: JUCE's own "no result
                                                   // id" convention (PopupMenu::Item's default), outside every band. It
                                                   // was 99999, which the uncapped 4000-based effect band could reach.

// Fixed action ids of the Add Track menu (showAddTrackMenu).
inline constexpr int kAudioTrack = 1000;
inline constexpr int kImportAudio = 1001;

// Where the uncapped plugin bands start. Every fixed action id above is below this (or negative), so a plugin
// item id can never equal an action id, whatever the catalog sizes. 4000 keeps the Add-effect band's historical base.
inline constexpr int kPluginBandFirst = 4000;

enum class TrackAction {
  none, rename, duplicate, removeTrack, automation, openInstrument,
  replaceWithPlugin, addPluginEffect, openEffect, removeEffect, toggleEffect
};
enum class AddTrackAction { none, addPluginInstrument, audioTrack, importAudio };

struct TrackChoice { TrackAction action = TrackAction::none; int index = -1; };
struct AddTrackChoice { AddTrackAction action = AddTrackAction::none; int index = -1; };

// Where the two uncapped plugin lists of the track menu start their item ids.
struct TrackMenuIds { int replacePluginFirst; int addEffectPluginFirst; };
inline TrackMenuIds trackMenuIds(int instrumentCount, int effectCount) {
  (void)instrumentCount;
  // The replace band stacks above the add-effect band: both grow with their uncapped catalogs and stay disjoint.
  return {kPluginBandFirst + effectCount, kPluginBandFirst};
}
// Where the uncapped plugin instrument list of the Add Track menu starts its item ids.
inline int addTrackPluginFirst() { return kPluginBandFirst; }

// The track menu's dispatch decision, in the callback's order (plugin branch first).
// chainCount is the track's effect chain size; an effect id past the chain does nothing, as in the callback.
inline TrackChoice decodeTrackMenu(int chosen, int instrumentCount, int effectCount, int chainCount) {
  const TrackMenuIds ids = trackMenuIds(instrumentCount, effectCount);
  if (chosen >= ids.replacePluginFirst && chosen - ids.replacePluginFirst < instrumentCount)
    return {TrackAction::replaceWithPlugin, chosen - ids.replacePluginFirst};
  if (chosen >= ids.addEffectPluginFirst && chosen - ids.addEffectPluginFirst < effectCount)
    return {TrackAction::addPluginEffect, chosen - ids.addEffectPluginFirst};
  if (chosen >= kEffectOpenFirst && chosen < kEffectIdEnd) {
    const int i = (chosen - kEffectOpenFirst) % kEffectIdSpan;
    if (i >= chainCount) return {};
    if (chosen < kEffectRemoveFirst) return {TrackAction::openEffect, i};
    if (chosen < kEffectOnFirst) return {TrackAction::removeEffect, i};
    return {TrackAction::toggleEffect, i};
  }
  if (chosen == kOpenInstrument) return {TrackAction::openInstrument, -1};
  if (chosen == kRename) return {TrackAction::rename, -1};
  if (chosen == kDuplicate) return {TrackAction::duplicate, -1};
  if (chosen == kRemoveTrack) return {TrackAction::removeTrack, -1};
  if (chosen == kAutomation) return {TrackAction::automation, -1};
  return {};
}

// The Add Track menu's dispatch decision, in the callback's order (plugin branch first).
inline AddTrackChoice decodeAddTrackMenu(int chosen, int instrumentCount) {
  const int first = addTrackPluginFirst();
  if (chosen >= first && chosen - first < instrumentCount)
    return {AddTrackAction::addPluginInstrument, chosen - first};
  if (chosen == kAudioTrack) return {AddTrackAction::audioTrack, -1};
  if (chosen == kImportAudio) return {AddTrackAction::importAudio, -1};
  return {};
}

}  // namespace core::app::menu