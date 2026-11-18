// Document operations used by the editors (M2): new projects and track management.
// Pure functions on the document; the caller commits the result to the History (one step each).
#pragma once
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "document.hpp"

namespace core::doc {

Document newEmptyDocument();
// Kick on every beat, rolling bass on the off-sixteenths, hats on the off-beats: 145 BPM, 16 bars.
Document newTemplateDocument();

// A free id from a base ("bass" -> "bass", "bass-2", ...), valid for the id rules.
std::string uniqueTrackId(const Document& doc, const std::string& base);

// Each returns the id of the track it created (or changed), or throws DocumentError.
std::string addTrack(Document& doc, const std::string& instrument, const std::string& name = "");
std::string duplicateTrack(Document& doc, const std::string& trackId);
void renameTrack(Document& doc, const std::string& trackId, const std::string& name);
// Removing a bus sends what fed it to where the bus went, and drops the sends to it.
void removeTrack(Document& doc, const std::string& trackId);
// The shown track is gone (deleted, or undone away): which track is shown now (deep hunt P06). One of the other
// selected tracks that are still there (the first in song order, taken out of alsoSelected), else the track that took
// its place in orderBefore (the one after it, or before it at the end), else "". Never a track nobody picked added to
// a multi-selection: Stage Core's Arrange acts on the selection. alsoSelected loses the tracks that are gone.
std::string shownAfterRemoval(const Document& now, const std::vector<std::string>& orderBefore, const std::string& removed,
                              std::set<std::string>& alsoSelected);

// ---- routing (M7a): buses, outputs and sends. Every change is checked (only to a bus, no loops).
std::string addBusTrack(Document& doc, const std::string& name = "");  // e.g. "Reverb" or "Drums"
void setTrackOutput(Document& doc, const std::string& trackId, const std::string& busId);  // "" = the master
std::string addSend(Document& doc, const std::string& trackId, const std::string& busId, double gainDb = 0, bool pre = false);
void setSend(Document& doc, const std::string& trackId, const std::string& sendId, double gainDb, bool pre);
void removeSend(Document& doc, const std::string& trackId, const std::string& sendId);

// ---- patterns and clips (one track)
Track& trackById(Document& doc, const std::string& trackId);  // throws DocumentError
Pattern& patternById(Track& track, const std::string& patternId);
std::string addPattern(Track& track, int64_t lengthTicks);  // a new empty pattern; returns its id
void clearPattern(Track& track, const std::string& patternId);  // every note goes; the pattern and its clips stay
// The pattern and every clip that plays it. A track keeps at least one pattern (clear that one instead).
void removePattern(Track& track, const std::string& patternId);
std::string addClip(Track& track, const std::string& patternId, int64_t startTick, int64_t lengthTicks);
// A whole-clip copy: phase, attachments and unknown fields stay with it; only id/start change.
std::string duplicateClip(Track& track, const std::string& clipId, int64_t startTick);
std::string pasteClip(Track& track, Clip copy, int64_t startTick);  // a copied clip (Ctrl+V), new id; its pattern must be there
void moveClip(Track& track, const std::string& clipId, int64_t startTick);
void resizeClip(Track& track, const std::string& clipId, int64_t lengthTicks);  // the pattern repeats inside
void removeClip(Track& track, const std::string& clipId);
// Gives the clip its own copy of a shared pattern (editing it no longer changes the other clips).
// Returns the clip's pattern id.
std::string makeClipUnique(Track& track, const std::string& clipId);
// Cuts a clip at atTick (strictly inside it): the second part keeps the pattern's phase. Returns its id.
std::string splitClip(Track& track, const std::string& clipId, int64_t atTick);
// Keeps at least one. `rackRefs`: pattern ids the Patterns window's slots reference on this track — they are
// the user's ideas even without a timeline clip, so they count as used (BUG-SWEEP-29).
void removeUnusedPatterns(Track& track, const std::set<std::string>& rackRefs = {});

// ---- effects (M4): plugin inserts on a track or on the master (trackId "")
constexpr int kMaxEffectsPerChain = 16;
// Reading an effect of a document that may have come from elsewhere: never throws on odd values.
std::string effectName(const json& fx);
bool effectBypassed(const json& fx);
double effectParam(const json& fx, const std::string& param, double fallback);
// One entry per chain effect for the menus that list a chain (the track's right-click, Plugins > Master
// effects). index: its place in the chain (removeEffect and setEffectBypass take the same);
// window: a plugin effect with a window to open; label: the name shown.
struct EffectMenuEntry { int index = 0; bool plugin = false; bool window = false; std::string label; };
std::vector<EffectMenuEntry> effectMenuEntries(const json& chain);
json& effectChain(Document& doc, const std::string& trackId);
const json& effectChain(const Document& doc, const std::string& trackId);
// The key of the song as a note name number 0..11 (0 = C), from the bass tracks (the notes they hold longest),
// or from every melodic track when there is no bass; -1 when the song has no notes.
int songRoot(const Document& doc);
// The note nearest to `note` (within 6 semitones) that is the song's root; `note` when there is no root.
int followSongKey(const Document& doc, int note);
void removeEffect(Document& doc, const std::string& trackId, int index);
void moveEffect(Document& doc, const std::string& trackId, int from, int to);
void setEffectBypass(Document& doc, const std::string& trackId, int index, bool bypass);

// ---- plugins (VST3, VST2): `plugin` is what the scan found {format, id, name, vendor, version, file, ...}
std::string addPluginTrack(Document& doc, const json& plugin);  // a track played by the plugin; returns its id
int addPluginEffect(Document& doc, const std::string& trackId, const json& plugin);  // at the end of the chain
// The track plays this plugin (the same one: nothing changes). Notes, clips, effects and the mixer stay.
void replaceInstrumentWithPlugin(Document& doc, const std::string& trackId, const json& plugin);
// Where the engine keeps a plugin: "track:<id>", or "fx:<track id or master>:<effect id>".
std::string pluginKey(const std::string& trackId, const std::string& effectId = "");
bool hasPlugins(const Document& doc);
// Every plugin place of the document (instruments and effects, by pluginKey): the keys the app asks the engine's
// plugins.states for (only the ones the song has, not every instance the engine ever loaded).
std::vector<std::string> pluginKeys(const Document& doc);
// The states of the named keys that the document holds ({key: base64}, non-empty states only): what the app answers
// the engine's project.load statesNeeded with (STATE-PER-EDIT: the projection carries no blobs).
json pluginStatesFor(const Document& doc, const std::vector<std::string>& keys);
// The states the engine gave ({key: base64}) written into the document; returns how many changed.
// changedKeys (optional): the keys whose state changed — what the app pushes back to the engine, whose stored
// copy is what an export makes its instances from (the projection itself carries no blobs any more).
int applyPluginStates(Document& doc, const json& states, std::vector<std::string>* changedKeys = nullptr);
// The plugins (instruments and effects, by pluginKey) that `before` has and `after` no longer has in the same
// place: gone, or another plugin (file or id) under the same key. The engine lets their instances go when it
// gets `after`, and what they hold is only there: it must be in the document first, or Undo brings them back
// as last saved.
std::vector<std::string> droppedPluginKeys(const Document& before, const Document& after);
// Undo and Redo never take back what a living plugin holds. `from` is the state being left, `to` the one Undo or
// Redo brings: every plugin that is in both under the same key and is the same plugin (file and id) keeps in `to`
// the state (and stateId) it has in `from`, the newest the app knows. The engine keeps that instance as it is, so
// an older state in the document would be saved, or loaded by a restarted engine, in place of the sound heard.
// A plugin the step brings back, or another plugin under the key, keeps the state of `to`. A plugin with no
// state in `from` takes nothing away from `to`. Returns how many plugins changed.
int keepLivePluginStates(const Document& from, Document& to);

// ---- automation (M4): a parameter that changes along the song, on a track or on the master (trackId "")
struct AutomatableParam {
  std::string param;   // "gainDb", "pan", "fx:<effect id>:<parameter>"
  std::string label;   // for the window
  double min, max, def;
  double current;      // the value set without automation (the fader, the effect's knob)
  std::string unit;
  bool logScale;
};
// What can be automated there: volume, pan (tracks) and the plugin parameters (a plugin's own curves are
// offered by the engine/window).
std::vector<AutomatableParam> automatableParams(const Document& doc, const std::string& trackId);
std::string effectParamKey(const std::string& effectId, const std::string& param);  // "fx:<id>:<param>"
const Automation* findAutomation(const Document& doc, const std::string& trackId, const std::string& param);
double automationValueAt(const Automation& a, int64_t tick);  // linear between points, held outside them
// Adds a point (or replaces the one at that tick); the value is kept in the parameter's range. Returns its index.
int setAutomationPoint(Document& doc, const std::string& trackId, const std::string& param, int64_t tick, double value);
// Moves a point; it stays between its neighbours (time order never changes). Returns its index.
int moveAutomationPoint(Document& doc, const std::string& trackId, const std::string& param, int index, int64_t tick, double value);
void removeAutomationPoint(Document& doc, const std::string& trackId, const std::string& param, int index);  // the last one removes the automation
void clearAutomation(Document& doc, const std::string& trackId, const std::string& param);

// Replaces the points of a curve in [startTick, endTick) and leaves the curve as it was outside: points are added
// at the edges with the values it had there (heldValue when there was no curve yet: the knob's value), so only
// the region sounds different. A point at startTick is kept by moving the left edge one tick earlier. Empty
// points: the curve goes straight from its old value at the start to its old value at the end.
void setAutomationInRange(Document& doc, const std::string& trackId, const std::string& param, int64_t startTick, int64_t endTick,
                          const std::vector<AutomationPoint>& points, double heldValue);

// ---- regions: a span of the song [startTick, endTick) on one track
struct RegionIsolation {
  std::vector<std::string> insideClips;               // the clips that now lie inside the region, in time order
  std::vector<std::pair<std::string, std::string>> patterns;  // pattern -> the region's own copy (only those that also play outside)
  int splitClips = 0;                                 // clips cut at an edge
};
// Makes the region editable on its own: the track's clips are cut at both edges (the second part keeps the
// pattern's phase, so nothing sounds different), then every pattern that plays both inside and outside gets a
// copy that only the clips inside use. Editing those copies (or the patterns used only inside) changes the
// region and nothing else. ids: wanted ids for the copies (pattern -> id), so a batch can name them; the rest get
// the next free id. Throws DocumentError (audio track, empty region, too many clips or patterns, an id taken).
RegionIsolation isolateRegion(Document& doc, const std::string& trackId, int64_t startTick, int64_t endTick,
                              const std::map<std::string, std::string>& ids = {});

// ---- audio tracks and clips (M3)
constexpr double kMinClipSeconds = 0.001;  // the shortest audio clip a document accepts
std::string addAudioTrack(Document& doc, const std::string& name);
// The whole asset from startTick; returns the clip id.
std::string addAudioClip(Document& doc, const std::string& trackId, const std::string& assetId, int64_t startTick);
void moveAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t startTick);

// Trim: the start moves with the music and the audio stays where it was (the offset follows); the end
// changes the duration. Both are limited to the file.
void trimAudioClipStart(Document& doc, const std::string& trackId, const std::string& clipId, int64_t startTick);
void trimAudioClipEnd(Document& doc, const std::string& trackId, const std::string& clipId, double durationSeconds);
// Two clips that play exactly what the one did. Returns the id of the second. A FROZEN clip
// (freezeSeconds > 0) is refused with an unfreeze offer (BUG-SWEEP-21, ratified): the split's offset
// re-anchor would make the second piece hold a different part of the file — never a half-frozen state.
std::string splitAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t atTick);
// Lets the frozen hold go: the clip plays its file normally again (the unfreeze the split refusal offers,
// one Undo step in the app). A clip without a freeze is unchanged.
void unfreezeAudioClip(Document& doc, const std::string& trackId, const std::string& clipId);
void removeAudioClip(Document& doc, const std::string& trackId, const std::string& clipId);
// The audio editor's selection (song ticks): keep only that part of the clip, or remove it (the clip
// becomes two, with a gap; at an edge, the clip just gets shorter).
void cropAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t fromTick, int64_t toTick);
// Cuts the clip at every multiple of stepTicks (the song's grid) inside it; returns how many clips it became.
int sliceAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t stepTicks);
void removeAudioRange(Document& doc, const std::string& trackId, const std::string& clipId, int64_t fromTick, int64_t toTick);
std::string duplicateAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t startTick);
std::string pasteAudioClip(Document& doc, const std::string& trackId, AudioClip copy, int64_t startTick);  // Ctrl+V, new id
// A clip dragged to another track (or the same one), at startTick; returns its id there. A pattern clip takes a
// copy of its pattern (the source keeps its own); an audio clip goes only to an audio track, a pattern clip only
// to an instrument track.
std::string moveClipToTrack(Document& doc, const std::string& fromTrack, const std::string& clipId, const std::string& toTrack, int64_t startTick);
double secondsPerTick(const Document& doc);  // at the song's start tempo
// Song time through the tempo map (the same conversion as the engine). With no tempo change these are
// tick * secondsPerTick and its inverse, exactly as before the map.
double secondsAt(const Document& doc, double tick);
double tickAtSeconds(const Document& doc, double seconds);
double secondsBetween(const Document& doc, double fromTick, double toTick);  // (to - from) * spt with one tempo
int64_t audioClipEndTick(const Document& doc, const AudioClip& clip);  // where it stops, in song ticks

// ---- sections (the whole song)
std::string addSection(Document& doc, const std::string& name, int64_t startTick, int64_t lengthTicks);
void renameSection(Document& doc, const std::string& sectionId, const std::string& name);
// "#rrggbb", or empty: back to the colour of its type (from the name).
void setSectionColor(Document& doc, const std::string& sectionId, const std::string& color);
void removeSection(Document& doc, const std::string& sectionId);  // the music stays
void swapSection(Document& doc, const std::string& sectionId, bool withNext);
std::string duplicateSection(Document& doc, const std::string& sectionId);
// Empty time at atTick: everything from there (clips, automation, sections, tempo changes, the loop) moves
// on by `ticks` and the song gets longer. Clips that cross atTick are split there; a section that
// crosses it grows.
void insertTime(Document& doc, int64_t atTick, int64_t ticks);
// Removes [fromTick, fromTick + ticks): the music there goes, everything after moves back, the song gets
// shorter. Clips and sections that cross an edge are cut there.
void deleteTime(Document& doc, int64_t fromTick, int64_t ticks);
void setSongLength(Document& doc, int64_t lengthTicks);  // tempo changes past the new end go
// The song's tempo changes (sorted here; ticks after the start and inside the song, BPM 40-600). Throws.
void setTempoMap(Document& doc, std::vector<TempoPoint> changes);
double bpmAt(const Document& doc, int64_t tick);  // the tempo playing at a tick

// ---- notes (one pattern)
std::string newNoteId(const Pattern& pattern);                 // n<highest + 1>
// Step grid: removes the notes of this pitch starting at this tick, or adds one. Returns true if added.
bool toggleStep(Pattern& pattern, int64_t tick, int pitch, int64_t length, double velocity);
// Copies the notes right after themselves (by the span they cover, snapped up to snapTicks), inside
// the pattern. Returns the ids of the copies.
std::vector<std::string> duplicateNotes(Pattern& pattern, const std::vector<std::string>& noteIds, int64_t snapTicks);
// Pattern length change: notes that no longer fit are removed, lengths are cut to the end.
void setPatternLength(Pattern& pattern, int64_t lengthTicks);
// Longer pattern with its notes repeated to fill the new bars (as a loop would play them); shorter or
// equal: the same as setPatternLength.
void extendPatternRepeating(Pattern& pattern, int64_t lengthTicks);

} // namespace core::doc
