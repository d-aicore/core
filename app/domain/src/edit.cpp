#include "edit.hpp"
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <deque>
#include <cstdlib>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace core::doc {

namespace {

Track& find(Document& doc, const std::string& id) {
  for (auto& t : doc.tracks) if (t.id == id) return t;
  throw DocumentError("tracks", "no track '" + id + "'");
}

Note note(int index, int64_t tick, int64_t length, int pitch, double velocity) {
  Note n;
  n.id = "n" + std::to_string(index);
  n.tick = tick; n.length = length; n.pitch = pitch; n.velocity = velocity;
  return n;
}

// Lower-case letters, digits and '-' from a name or an instrument, for a readable id.
std::string slug(const std::string& text) {
  std::string out;
  for (unsigned char c : text) {
    if (std::isalnum(c)) out += (char)std::tolower(c);
    else if (!out.empty() && out.back() != '-') out += '-';
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  if (out.empty()) out = "track";
  return out.substr(0, 32);
}

// Whether the text is well-formed UTF-8: the shortest form only, no surrogate halves, nothing above U+10FFFF, no
// cut sequence. nlohmann's dump refuses anything else, and a song holding such a name cannot be saved
// (BUG-SWEEP-18).
bool wellFormedUtf8(const std::string& s) {
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = (unsigned char)s[i];
    const size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (len == 0 || i + len > s.size() || (len == 2 && c < 0xC2)) return false;
    for (size_t k = 1; k < len; ++k) if (((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
    if (len == 3 && ((c == 0xE0 && (unsigned char)s[i + 1] < 0xA0) || (c == 0xED && (unsigned char)s[i + 1] >= 0xA0))) return false;
    if (len == 4 && (c > 0xF4 || (c == 0xF0 && (unsigned char)s[i + 1] < 0x90) || (c == 0xF4 && (unsigned char)s[i + 1] >= 0x90))) return false;
    i += len;
  }
  return true;
}

} // namespace


Document newEmptyDocument() {
  Document d;
  d.name = "";
  d.bpm = 145;
  d.lengthTicks = 16 * 4 * kPpq;
  d.loop = {false, 0, 4 * 4 * kPpq, json::object()};
  // Headroom: an empty song has no limiter on the master, and kick + bass + hats at their default levels (P-Core
  // writes them so) peaked at 0.95..1.44 with the master at 0 dB (audit 2026-11-08, part 17). At -6 dB they stay
  // under 0 dBFS, as in the basic template.
  d.masterGainDb = -6;
  return d;
}

Document newTemplateDocument() {
  Document d = newEmptyDocument();
  const std::string kick = addTrack(d, "kick", "Kick"), bass = addTrack(d, "bass", "Bass"), hats = addTrack(d, "hats", "Hats");
  Track& k = find(d, kick);
  k.color.clear();  // the role colour (TrackColour), the same as in every song
  for (int beat = 0; beat < 4; ++beat) k.patterns[0].notes.push_back(note(beat + 1, beat * kPpq, kPpq / 4, 36, 0.95));
  Track& b = find(d, bass);
  b.color.clear();
  b.mix.gainDb = -3;
  int i = 0;
  for (int beat = 0; beat < 4; ++beat)
    for (int sixteenth = 1; sixteenth < 4; ++sixteenth)
      b.patterns[0].notes.push_back(note(++i, beat * kPpq + sixteenth * kPpq / 4, kPpq / 4 - 40, 33, 0.8));
  Track& h = find(d, hats);
  h.color.clear();
  h.mix.gainDb = -6;
  for (int beat = 0; beat < 4; ++beat) h.patterns[0].notes.push_back(note(beat + 1, beat * kPpq + kPpq / 2, 120, 46, 0.7));
  return d;
}


std::string uniqueTrackId(const Document& doc, const std::string& base) {
  const std::string root = slug(base);
  std::string id = root;
  // "master" is the master's name in plugin keys and routing (audit B02): a track never takes it.
  for (int n = 2; doc.findTrack(id) || id == "master"; ++n) id = root + "-" + std::to_string(n);
  return id;
}

// A track for notes with no instrument yet (Diogo 2026-11-16: the sound comes from plugins): its notes play once a
// plugin is chosen for it. The role names it ("kick", "bass"...) when no name is given.
std::string addTrack(Document& doc, const std::string& role, const std::string& name) {
  if (doc.tracks.size() >= 128) throw DocumentError("tracks", "has the maximum of 128 tracks");
  Track t;
  t.id = uniqueTrackId(doc, role.empty() ? std::string("track") : role);
  t.name = cutUtf8(name.empty() ? (role.empty() ? std::string("Track") : role) : name, 80);  // never half a character
  t.instrument.kind = "none";
  // One 1-bar pattern played over the whole song: edit it and hear it at once.
  Pattern p;
  p.id = "p1";
  p.name = "1";
  p.lengthTicks = 4 * kPpq;
  t.patterns.push_back(p);
  t.clips.push_back({"c1", "p1", 0, doc.lengthTicks, 0, json::object()});
  doc.tracks.push_back(std::move(t));
  return doc.tracks.back().id;
}

std::string duplicateTrack(Document& doc, const std::string& trackId) {
  if (doc.tracks.size() >= 128) throw DocumentError("tracks", "has the maximum of 128 tracks");
  const auto it = std::find_if(doc.tracks.begin(), doc.tracks.end(), [&](const Track& t) { return t.id == trackId; });
  if (it == doc.tracks.end()) throw DocumentError("tracks", "no track '" + trackId + "'");
  Track copy = *it;  // notes keep their ids: they are unique within their own pattern
  copy.id = uniqueTrackId(doc, trackId);
  copy.name = nameWithSuffix(it->name, " 2", kMaxTrackNameBytes);
  copy.mix.solo = false;
  const auto at = it - doc.tracks.begin() + 1;  // right after the original
  doc.tracks.insert(doc.tracks.begin() + at, std::move(copy));
  return doc.tracks[(size_t)at].id;
}

std::string shownAfterRemoval(const Document& now, const std::vector<std::string>& orderBefore, const std::string& removed,
                              std::set<std::string>& alsoSelected) {
  for (auto it = alsoSelected.begin(); it != alsoSelected.end();) it = now.findTrack(*it) ? std::next(it) : alsoSelected.erase(it);
  for (const auto& t : now.tracks)
    if (alsoSelected.count(t.id)) { alsoSelected.erase(t.id); return t.id; }
  const auto at = std::find(orderBefore.begin(), orderBefore.end(), removed);
  if (at != orderBefore.end()) {
    for (auto it = at + 1; it != orderBefore.end(); ++it) if (now.findTrack(*it)) return *it;
    for (auto it = at; it != orderBefore.begin();) if (now.findTrack(*--it)) return *it;
  }
  return now.tracks.empty() ? std::string() : now.tracks.front().id;
}

void renameTrack(Document& doc, const std::string& trackId, const std::string& name) {
  std::string clean = name;
  clean.erase(0, clean.find_first_not_of(" \t"));
  clean.erase(clean.find_last_not_of(" \t") + 1);
  if (clean.empty()) throw DocumentError("name", "cannot be empty");
  if (clean.size() > 80) throw DocumentError("name", "is longer than 80 bytes (an accented letter counts as 2)");
  if (!wellFormedUtf8(clean)) throw DocumentError("name", "is not valid UTF-8 text");  // it would make the song unsavable (BUG-SWEEP-18)
  find(doc, trackId).name = clean;  // the id stays: it is what links meters, history and the engine
}

Track& trackById(Document& doc, const std::string& trackId) { return find(doc, trackId); }

std::string newNoteId(const Pattern& pattern) {
  long highest = 0;
  for (const auto& n : pattern.notes)
    if (n.id.size() > 1 && n.id[0] == 'n') highest = std::max(highest, idNumber(n.id.c_str() + 1));
  return "n" + std::to_string(highest + 1);
}

bool toggleStep(Pattern& pattern, int64_t tick, int pitch, int64_t length, double velocity) {
  const auto before = pattern.notes.size();
  pattern.notes.erase(std::remove_if(pattern.notes.begin(), pattern.notes.end(),
                                     [&](const Note& n) { return n.tick == tick && n.pitch == pitch; }),
                      pattern.notes.end());
  if (pattern.notes.size() != before) return false;
  if (tick < 0 || tick >= pattern.lengthTicks) throw DocumentError("tick", "outside the pattern");
  if (pattern.notes.size() >= 4096) throw DocumentError("notes", "the pattern has the maximum of 4096 notes");
  Note n;
  n.id = newNoteId(pattern);
  n.tick = tick;
  n.length = std::max<int64_t>(1, std::min(length, pattern.lengthTicks - tick));
  n.pitch = pitch;
  n.velocity = velocity;
  pattern.notes.push_back(n);
  return true;
}

std::vector<std::string> duplicateNotes(Pattern& pattern, const std::vector<std::string>& noteIds, int64_t snapTicks) {
  int64_t first = INT64_MAX, last = 0;
  std::vector<Note> chosen;
  for (const auto& n : pattern.notes)
    if (std::find(noteIds.begin(), noteIds.end(), n.id) != noteIds.end()) {
      chosen.push_back(n);
      first = std::min(first, n.tick);
      last = std::max(last, n.tick + n.length);
    }
  std::vector<std::string> copies;
  if (chosen.empty()) return copies;
  const int64_t snap = std::max<int64_t>(1, snapTicks);
  const int64_t span = (last - first + snap - 1) / snap * snap;
  for (Note n : chosen) {
    n.tick += span;
    if (n.tick >= pattern.lengthTicks) continue;  // past the end of the pattern: not copied
    if (pattern.notes.size() >= 4096) break;
    n.length = std::min(n.length, pattern.lengthTicks - n.tick);
    n.id = newNoteId(pattern);
    pattern.notes.push_back(n);
    copies.push_back(n.id);
  }
  return copies;
}

void setPatternLength(Pattern& pattern, int64_t lengthTicks) {
  if (lengthTicks < kPpq / 8 || lengthTicks > 64LL * 4 * kPpq) throw DocumentError("lengthTicks", "invalid pattern length");
  pattern.lengthTicks = lengthTicks;
  pattern.notes.erase(std::remove_if(pattern.notes.begin(), pattern.notes.end(), [&](const Note& n) { return n.tick >= lengthTicks; }),
                      pattern.notes.end());
  for (auto& n : pattern.notes) n.length = std::min(n.length, lengthTicks - n.tick);
}

void extendPatternRepeating(Pattern& pattern, int64_t lengthTicks) {
  const int64_t old = pattern.lengthTicks;
  const std::vector<Note> source = pattern.notes;
  setPatternLength(pattern, lengthTicks);
  if (lengthTicks <= old || old <= 0 || source.empty()) return;
  for (int64_t offset = old; offset < lengthTicks; offset += old)
    for (Note n : source) {
      n.tick += offset;
      if (n.tick >= lengthTicks) continue;
      if (pattern.notes.size() >= 4096) return;
      n.length = std::min(n.length, lengthTicks - n.tick);
      n.id = newNoteId(pattern);
      pattern.notes.push_back(n);
    }
}

Pattern& patternById(Track& track, const std::string& patternId) {
  if (Pattern* p = track.findPattern(patternId)) return *p;
  throw DocumentError("patterns", "no pattern '" + patternId + "'");
}

namespace {
// Next free "<prefix><n>" among existing ids.
template <typename List>
std::string nextId(const List& items, const std::string& prefix) {
  long highest = 0;
  for (const auto& x : items)
    if (x.id.rfind(prefix, 0) == 0) highest = std::max(highest, idNumber(x.id.c_str() + prefix.size()));
  return prefix + std::to_string(highest + 1);
}
Clip& clipById(Track& t, const std::string& clipId) {
  for (auto& c : t.clips) if (c.id == clipId) return c;
  throw DocumentError("clips", "no clip '" + clipId + "'");
}
void sortClips(Track& t) {
  std::stable_sort(t.clips.begin(), t.clips.end(), [](const Clip& a, const Clip& b) { return a.startTick < b.startTick; });
}
}  // namespace

std::string addPattern(Track& track, int64_t lengthTicks) {
  if (track.patterns.size() >= 64) throw DocumentError("patterns", "the track has the maximum of 64 patterns");
  // The reader's range (1/32 .. 64 bars): a 0 length divided by zero in the projection (sweep).
  if (lengthTicks < kPpq / 8 || lengthTicks > 64LL * 4 * kPpq) throw DocumentError("patterns", "a pattern is 1/32 to 64 bars long");
  Pattern p;
  p.id = nextId(track.patterns, "p");
  p.name = std::to_string(track.patterns.size() + 1);
  p.lengthTicks = lengthTicks;
  track.patterns.push_back(p);
  return p.id;
}

void clearPattern(Track& track, const std::string& patternId) { patternById(track, patternId).notes.clear(); }

void removePattern(Track& track, const std::string& patternId) {
  patternById(track, patternId);
  if (track.patterns.size() <= 1) throw DocumentError("patterns", "a track keeps at least one pattern (clear it instead)");
  track.clips.erase(std::remove_if(track.clips.begin(), track.clips.end(), [&](const Clip& c) { return c.patternId == patternId; }), track.clips.end());
  track.patterns.erase(std::remove_if(track.patterns.begin(), track.patterns.end(), [&](const Pattern& p) { return p.id == patternId; }), track.patterns.end());
}

std::string addClip(Track& track, const std::string& patternId, int64_t startTick, int64_t lengthTicks) {
  if (!track.findPattern(patternId)) throw DocumentError("patternId", "no pattern '" + patternId + "'");
  if (track.clips.size() >= 256) throw DocumentError("clips", "the track has the maximum of 256 clips");
  if (startTick < 0 || lengthTicks < 1) throw DocumentError("clips", "invalid position");
  Clip c{nextId(track.clips, "c"), patternId, startTick, lengthTicks, 0, json::object()};
  track.clips.push_back(c);
  sortClips(track);
  return c.id;
}

std::string duplicateClip(Track& track, const std::string& clipId, int64_t startTick) {
  Clip copy = clipById(track, clipId);
  if (track.clips.size() >= 256) throw DocumentError("clips", "the track has the maximum of 256 clips");
  if (startTick < 0) throw DocumentError("clips", "invalid position");
  copy.id = nextId(track.clips, "c");
  copy.startTick = startTick;
  const std::string id = copy.id;
  track.clips.push_back(std::move(copy));
  sortClips(track);
  return id;
}

// A copied clip put back on its track (Ctrl+V, also after a cut: the source may be gone). Its pattern must still be on
// the track; the copy gets a new id.
std::string pasteClip(Track& track, Clip copy, int64_t startTick) {
  patternById(track, copy.patternId);  // throws when the pattern is gone
  if (track.clips.size() >= 256) throw DocumentError("clips", "the track has the maximum of 256 clips");
  copy.id = nextId(track.clips, "c");
  copy.startTick = std::max<int64_t>(0, startTick);
  const std::string id = copy.id;
  track.clips.push_back(std::move(copy));
  sortClips(track);
  return id;
}

void moveClip(Track& track, const std::string& clipId, int64_t startTick) {
  clipById(track, clipId).startTick = std::max<int64_t>(0, startTick);
  sortClips(track);
}

void resizeClip(Track& track, const std::string& clipId, int64_t lengthTicks) {
  clipById(track, clipId).lengthTicks = std::max<int64_t>(1, lengthTicks);
}

void removeClip(Track& track, const std::string& clipId) {
  clipById(track, clipId);
  track.clips.erase(std::remove_if(track.clips.begin(), track.clips.end(), [&](const Clip& c) { return c.id == clipId; }), track.clips.end());
}

std::string makeClipUnique(Track& track, const std::string& clipId) {
  Clip& clip = clipById(track, clipId);
  const Pattern& shared = patternById(track, clip.patternId);
  if (track.clipsUsing(shared.id) <= 1) return shared.id;  // already its own
  if (track.patterns.size() >= 64) throw DocumentError("patterns", "the track has the maximum of 64 patterns");
  Pattern copy = shared;  // note ids stay: they are unique within each pattern
  copy.id = nextId(track.patterns, "p");
  copy.name = nameWithSuffix(shared.name, "b", kMaxPatternNameBytes);
  track.patterns.push_back(copy);
  clipById(track, clipId).patternId = copy.id;  // the reference above may have moved
  return copy.id;
}

std::string splitClip(Track& track, const std::string& clipId, int64_t atTick) {
  const Clip c = clipById(track, clipId);
  if (atTick <= c.startTick || atTick >= c.startTick + c.lengthTicks) throw DocumentError("clips", "the cut is not inside the clip");
  const std::string second = duplicateClip(track, clipId, atTick);
  clipById(track, second).lengthTicks = c.startTick + c.lengthTicks - atTick;
  const int64_t len = std::max<int64_t>(1, patternById(track, c.patternId).lengthTicks);
  clipById(track, second).offsetTicks = (c.offsetTicks + atTick - c.startTick) % len;
  clipById(track, clipId).lengthTicks = atTick - c.startTick;
  return second;
}

void removeUnusedPatterns(Track& track, const std::set<std::string>& rackRefs) {
  if (track.patterns.size() <= 1) return;
  std::vector<Pattern> kept;
  for (auto& p : track.patterns) if (track.clipsUsing(p.id) > 0 || rackRefs.count(p.id) > 0) kept.push_back(std::move(p));
  if (kept.empty()) kept.push_back(track.patterns.front());
  track.patterns = std::move(kept);
}

// ---- regions

RegionIsolation isolateRegion(Document& doc, const std::string& trackId, int64_t startTick, int64_t endTick,
                              const std::map<std::string, std::string>& ids) {
  Track& t = find(doc, trackId);
  if (!playsPatterns(t)) throw DocumentError("region", "track '" + trackId + "' has no patterns (audio track or bus)");
  if (startTick < 0 || endTick <= startTick) throw DocumentError("region", "the region must end after it starts");
  RegionIsolation out;
  // 1. Cut the clips that cross an edge: the second part continues the pattern where the first stops.
  for (int64_t at : {startTick, endTick}) {
    std::vector<Clip> added;
    for (auto& c : t.clips)
      if (c.startTick < at && c.startTick + c.lengthTicks > at) {
        Clip second = c;
        second.startTick = at;
        second.lengthTicks = c.startTick + c.lengthTicks - at;
        const Pattern* p = t.findPattern(c.patternId);
        const int64_t len = p ? std::max<int64_t>(1, p->lengthTicks) : 1;
        second.offsetTicks = (c.offsetTicks + (at - c.startTick)) % len;  // its other fields stay (sweep: they were wiped)
        c.lengthTicks = at - c.startTick;
        added.push_back(second);
      }
    if (t.clips.size() + added.size() > 256) throw DocumentError("clips", "the track would have more than 256 clips");
    for (auto& a : added) { a.id = nextId(t.clips, "c"); t.clips.push_back(a); ++out.splitClips; }
  }
  sortClips(t);
  // 2. Which patterns play inside, and which of them also play outside.
  auto inside = [&](const Clip& c) { return c.startTick >= startTick && c.startTick + c.lengthTicks <= endTick; };
  std::vector<std::string> shared;
  for (const auto& c : t.clips) {
    if (!inside(c)) continue;
    out.insideClips.push_back(c.id);
    if (std::find(shared.begin(), shared.end(), c.patternId) != shared.end()) continue;
    if (std::any_of(t.clips.begin(), t.clips.end(), [&](const Clip& o) { return !inside(o) && o.patternId == c.patternId; }))
      shared.push_back(c.patternId);
  }
  for (const auto& [from, to] : ids)
    if (std::find(shared.begin(), shared.end(), from) == shared.end())
      throw DocumentError("region", "pattern '" + from + "' does not play both inside and outside the region, so it gets no copy");
  // 3. One copy per shared pattern, used by every clip inside.
  if (t.patterns.size() + shared.size() > 64) throw DocumentError("patterns", "the track would have more than 64 patterns");
  static const std::regex idRule("^[A-Za-z0-9_-]{1,40}$");
  for (const auto& pid : shared) {
    Pattern copy = patternById(t, pid);  // note ids stay: they are unique within each pattern
    auto wanted = ids.find(pid);
    if (wanted != ids.end()) {
      if (!std::regex_match(wanted->second, idRule)) throw DocumentError("region", "'" + wanted->second + "' is not a valid pattern id");
      if (t.findPattern(wanted->second)) throw DocumentError("region", "track '" + trackId + "' already has a pattern '" + wanted->second + "'");
      copy.id = wanted->second;
    } else {
      copy.id = nextId(t.patterns, "p");
    }
    copy.name = copy.name + " region";
    if (copy.name.size() > 40) {  // never cut inside a character (a broken name made the song unsavable)
      size_t cut = 40;
      while (cut > 0 && ((unsigned char)copy.name[cut] & 0xC0) == 0x80) --cut;
      copy.name.resize(cut);
    }
    t.patterns.push_back(copy);
    for (auto& c : t.clips) if (inside(c) && c.patternId == pid) c.patternId = copy.id;
    out.patterns.emplace_back(pid, copy.id);
  }
  return out;
}

// ---- sections

namespace {
Section& sectionById(Document& doc, const std::string& id) {
  for (auto& s : doc.sections) if (s.id == id) return s;
  throw DocumentError("sections", "no section '" + id + "'");
}
void sortSections(Document& doc) {
  std::stable_sort(doc.sections.begin(), doc.sections.end(), [](const Section& a, const Section& b) { return a.startTick < b.startTick; });
}
// Splits every clip that crosses tick into two clips (same pattern, the second restarts the pattern).
void splitClipsAt(Document& doc, int64_t tick) {
  for (auto& t : doc.tracks)
    for (size_t i = 0; i < t.audioClips.size(); ++i) {
      const AudioClip& a = t.audioClips[i];
      const double cut = secondsBetween(doc, (double)a.startTick, (double)tick);
      if (cut >= kMinClipSeconds && a.durationSeconds - cut >= kMinClipSeconds) splitAudioClip(doc, t.id, a.id, tick);
    }
  for (auto& t : doc.tracks) {
    std::vector<Clip> added;
    for (auto& c : t.clips)
      if (c.startTick < tick && c.startTick + c.lengthTicks > tick) {
        Clip second = c;
        second.startTick = tick;
        second.lengthTicks = c.startTick + c.lengthTicks - tick;
        // The second part continues the pattern where the first one stops, so nothing sounds different.
        const Pattern* p = t.findPattern(c.patternId);
        const int64_t len = p ? std::max<int64_t>(1, p->lengthTicks) : 1;
        second.offsetTicks = (c.offsetTicks + (tick - c.startTick)) % len;
        c.lengthTicks = tick - c.startTick;
        added.push_back(second);
      }
    for (auto& a : added) { a.id = nextId(t.clips, "c"); t.clips.push_back(a); }
    sortClips(t);
  }
}
// Every automation list of the song: each track's and the master's.
std::vector<std::vector<Automation>*> automationLists(Document& doc) {
  std::vector<std::vector<Automation>*> lists;
  for (auto& t : doc.tracks) lists.push_back(&t.automation);
  lists.push_back(&doc.masterAutomation);
  return lists;
}

// Automation points are in song ticks: a point is added at tick (the value there, interpolated) so
// the curve keeps its shape when the music on either side moves.
void splitAutomationAt(Document& doc, int64_t tick) {
  for (auto* list : automationLists(doc))
    for (auto& a : *list) {
      // Two points: just before the boundary (the left side keeps its own end value) and on it.
      for (int64_t at : {tick - 1, tick}) {
        if (a.points.empty() || at <= a.points.front().tick || at >= a.points.back().tick) continue;
        auto it = std::lower_bound(a.points.begin(), a.points.end(), at, [](const AutomationPoint& p, int64_t v) { return p.tick < v; });
        if (it->tick == at) continue;
        const AutomationPoint& b = *it;
        const AutomationPoint& prev = *(it - 1);
        const double v = prev.value + (b.value - prev.value) * double(at - prev.tick) / double(b.tick - prev.tick);
        a.points.insert(it, AutomationPoint{at, v, json::object()});
      }
    }
}
void sortAutomation(Document& doc) {
  for (auto* list : automationLists(doc))
    for (auto& a : *list)
      std::stable_sort(a.points.begin(), a.points.end(), [](const AutomationPoint& x, const AutomationPoint& y) { return x.tick < y.tick; });
}
// Tempo's boundary split (the tempo twin of splitAutomationAt): a glide that crosses `tick` gets an
// interpolated point there, collinear with the glide, so bpmAt does not change at any tick and the curve
// on either side keeps its exact shape when the music moves (BUG-SWEEP-22..24).
void splitTempoRampAt(Document& doc, int64_t tick) {
  if (tick < 1 || tick >= doc.lengthTicks) return;
  for (size_t i = 0; i < doc.tempoMap.size(); ++i) {
    const TempoPoint& p = doc.tempoMap[i];
    if (p.tick == tick) return;
    if (p.tick > tick) {
      const int64_t origin = i ? doc.tempoMap[i - 1].tick : 0;
      if (p.ramp && origin < tick) {
        TempoPoint at = p;
        at.tick = tick;
        at.bpm = bpmAt(doc, tick);
        doc.tempoMap.insert(doc.tempoMap.begin() + (long)i, at);
      }
      return;
    }
  }
}
}  // namespace

std::string addSection(Document& doc, const std::string& name, int64_t startTick, int64_t lengthTicks) {
  if (doc.sections.size() >= 64) throw DocumentError("sections", "the song has the maximum of 64 sections");
  if (startTick < 0 || lengthTicks < 1) throw DocumentError("sections", "invalid position");
  for (const auto& s : doc.sections)
    if (startTick < s.startTick + s.lengthTicks && s.startTick < startTick + lengthTicks)
      throw DocumentError("sections", "overlaps the section '" + s.name + "'");
  Section s{nextId(doc.sections, "s"), cutUtf8(name, kMaxSectionNameBytes), startTick, lengthTicks, json::object()};  // never half a character (sweep D6)
  doc.sections.push_back(s);
  sortSections(doc);
  return s.id;
}

void renameSection(Document& doc, const std::string& sectionId, const std::string& name) {
  if (name.empty() || name.size() > 40) throw DocumentError("name", "must have 1-40 characters");
  sectionById(doc, sectionId).name = name;
}

void setSectionColor(Document& doc, const std::string& sectionId, const std::string& color) {
  static const std::regex pattern("^#[0-9A-Fa-f]{6}$");
  if (!color.empty() && !std::regex_match(color, pattern)) throw DocumentError("sections", "a colour must look like #rrggbb");
  sectionById(doc, sectionId).color = color;
}

void removeSection(Document& doc, const std::string& sectionId) {
  sectionById(doc, sectionId);
  doc.sections.erase(std::remove_if(doc.sections.begin(), doc.sections.end(), [&](const Section& s) { return s.id == sectionId; }),
                     doc.sections.end());
}

// Exchanges the music of a section with the next (or previous) one: both must touch. Clips that cross
// the boundaries are split there first; the song length does not change.
void swapSection(Document& doc, const std::string& sectionId, bool withNext) {
  sortSections(doc);
  auto it = std::find_if(doc.sections.begin(), doc.sections.end(), [&](const Section& s) { return s.id == sectionId; });
  if (it == doc.sections.end()) throw DocumentError("sections", "no section '" + sectionId + "'");
  if (withNext ? it + 1 == doc.sections.end() : it == doc.sections.begin()) throw DocumentError("sections", "no section on that side");
  Section& a = withNext ? *it : *(it - 1);
  Section& b = withNext ? *(it + 1) : *it;
  if (a.startTick + a.lengthTicks != b.startTick) throw DocumentError("sections", "only sections that touch can swap");
  const int64_t aStart = a.startTick, bStart = b.startTick, end = b.startTick + b.lengthTicks;
  const int64_t aLen = bStart - aStart, bLen = end - bStart;
  // Tempo goes with the music (BUG-SWEEP-23, the policy duplicateSection already follows): each section's
  // curve moves to the other's place with its effective start value pinned, and the music outside the swap
  // keeps its curve. Built before anything moves: a map over the cap throws with the song untouched.
  const double vA = bpmAt(doc, aStart), vB = bpmAt(doc, bStart), vEnd = bpmAt(doc, end);
  const double vBMinus = bpmAt(doc, bStart - 1), vEndMinus = bpmAt(doc, end - 1);
  const auto firstFrom = [&](int64_t tick) -> const TempoPoint* {
    for (const auto& p : doc.tempoMap) if (p.tick >= tick) return &p;
    return nullptr;
  };
  const TempoPoint* atA = firstFrom(aStart);
  const TempoPoint* atB = firstFrom(bStart);
  const TempoPoint* atEnd = firstFrom(end);
  const bool glideAtA = atA && atA->ramp;        // a glide arriving at or crossing aStart
  const bool glideAtB = atB && atB->ramp;        // ... bStart (the tail of A)
  const bool glideAtEnd = atEnd && atEnd->ramp;  // ... end (the tail of B)
  const TempoPoint* afterEnd = nullptr;
  for (const auto& p : doc.tempoMap) if (p.tick > end) { afterEnd = &p; break; }
  const bool glideCrossesEnd = afterEnd && afterEnd->ramp;
  std::vector<TempoPoint> tempo;
  double baseBpm = doc.bpm;
  if (!doc.tempoMap.empty()) {
    const auto taken = [&](int64_t tick) { for (const auto& p : tempo) if (p.tick == tick) return true; return false; };
    // Before the swapped region: unchanged; a glide crossing aStart keeps its shape up to aStart - 1.
    for (const auto& p : doc.tempoMap) if (p.tick < aStart) tempo.push_back(p);
    if (glideAtA && aStart >= 2 && !taken(aStart - 1)) tempo.push_back({aStart - 1, bpmAt(doc, aStart - 1), json::object(), true});
    // B's music comes first: its curve translated by -aLen, starting at its own effective value.
    for (const auto& p : doc.tempoMap)
      if (p.tick > bStart && p.tick < end) tempo.push_back({p.tick - aLen, p.bpm, p.extra, p.ramp});
    const TempoPoint* atBStart = nullptr;
    for (const auto& p : doc.tempoMap) if (p.tick == bStart) { atBStart = &p; break; }
    if (aStart == 0) baseBpm = atBStart ? atBStart->bpm : vB;  // tick 0 is the song's bpm, not a point
    else if (atBStart) tempo.push_back({aStart, atBStart->bpm, atBStart->extra, false});
    else tempo.push_back({aStart, vB, json::object(), false});
    if (glideAtEnd && bLen > 1 && !taken(aStart + bLen - 1))
      tempo.push_back({aStart + bLen - 1, vEndMinus, json::object(), true});  // B's tail glide, cut at end
    // A's music follows: its curve translated by +bLen, starting at its own effective value.
    for (const auto& p : doc.tempoMap)
      if (p.tick > aStart && p.tick < bStart) tempo.push_back({p.tick + bLen, p.bpm, p.extra, p.ramp});
    const TempoPoint* atAStart = nullptr;
    for (const auto& p : doc.tempoMap) if (p.tick == aStart) { atAStart = &p; break; }
    if (atAStart) tempo.push_back({aStart + bLen, atAStart->bpm, atAStart->extra, false});
    else tempo.push_back({aStart + bLen, vA, json::object(), false});
    if (glideAtB && aLen > 1 && !taken(end - 1))
      tempo.push_back({end - 1, vBMinus, json::object(), true});  // A's tail glide, cut at bStart
    // After the swapped region: unchanged, with the value the music had at end pinned there.
    for (const auto& p : doc.tempoMap) if (p.tick == end) tempo.push_back({end, p.bpm, p.extra, false});
    for (const auto& p : doc.tempoMap) if (p.tick > end) tempo.push_back(p);
    if (end < doc.lengthTicks && !taken(end) && (glideCrossesEnd || std::abs(vBMinus - vEnd) > 1e-9))
      tempo.push_back({end, vEnd, json::object(), false});
    if (tempo.size() > (size_t)kMaxTempoPoints)
      throw DocumentError("tempoMap", "at most " + std::to_string(kMaxTempoPoints) + " tempo changes");
    std::sort(tempo.begin(), tempo.end(), [](const TempoPoint& x, const TempoPoint& y) { return x.tick < y.tick; });
  }
  for (int64_t at : {aStart, bStart, end}) { splitClipsAt(doc, at); splitAutomationAt(doc, at); }
  for (auto* list : automationLists(doc))
    for (auto& a : *list)
      for (auto& p : a.points) {
        // Each section's curve, from its start point to the point just before its end, moves with it.
        if (p.tick >= aStart && p.tick < bStart) p.tick += bLen;
        else if (p.tick >= bStart && p.tick < end) p.tick -= aLen;
      }
  sortAutomation(doc);
  for (auto& t : doc.tracks) {
    for (auto& c : t.clips) {
      if (c.startTick >= aStart && c.startTick < bStart) c.startTick += bLen;       // A moves after B
      else if (c.startTick >= bStart && c.startTick < end) c.startTick -= aLen;     // B moves to A's place
    }
    for (auto& c : t.audioClips) {
      if (c.startTick >= aStart && c.startTick < bStart) c.startTick += bLen;
      else if (c.startTick >= bStart && c.startTick < end) c.startTick -= aLen;
    }
    sortClips(t);
  }
  std::swap(a.name, b.name);
  std::swap(a.extra, b.extra);
  std::swap(a.color, b.color);  // the colour goes with the name
  std::swap(a.id, b.id);
  a.lengthTicks = bLen;
  b.startTick = aStart + bLen;
  b.lengthTicks = aLen;
  sortSections(doc);
  doc.tempoMap = std::move(tempo);
  doc.bpm = baseBpm;
}

// Inserts a copy of the section right after it: everything later moves on, the song gets longer.
std::string duplicateSection(Document& doc, const std::string& sectionId) {
  const Section original = sectionById(doc, sectionId);
  const int64_t start = original.startTick, len = original.lengthTicks, end = start + len;
  if (doc.lengthTicks + len > 4096LL * 4 * kPpq) throw DocumentError("lengthTicks", "the song would be too long");
  if (doc.sections.size() >= 64) throw DocumentError("sections", "the song has the maximum of 64 sections");
  // The section's tempo changes go with its music (audit 2026-11-17, #4: they did not, and a duplicated section
  // lost the tempo Randomize axis J wrote for it). Decided here, before anything moves.
  std::vector<TempoPoint> tempoCopies;
  for (const auto& p : doc.tempoMap)
    if (p.tick >= start && p.tick < end) tempoCopies.push_back({p.tick + len, p.bpm, p.extra, p.ramp});
  if (doc.tempoMap.size() + tempoCopies.size() > (size_t)kMaxTempoPoints)
    throw DocumentError("tempoMap", "at most " + std::to_string(kMaxTempoPoints) + " tempo changes");
  // All or nothing (the same discipline as importMidi): a track over its 256 clips used to leave the automation,
  // the tempo map and the earlier tracks already moved.
  Document next = doc;
  splitClipsAt(next, start);
  splitClipsAt(next, end);
  splitAutomationAt(next, start);
  splitAutomationAt(next, end);
  // Tempo boundaries (BUG-SWEEP-22): the copy must start with the section's own effective tempo, and a
  // glide crossing either edge keeps its shape on both sides. The values are read before anything moves.
  const double vStart = bpmAt(doc, start);
  const double vEnd = bpmAt(doc, end);
  const double vEndMinus = bpmAt(doc, end - 1);
  bool glideAtEnd = false;  // a glide is in progress where the section ends (arriving at end or crossing it)
  for (const auto& p : doc.tempoMap) if (p.tick >= end) { glideAtEnd = p.ramp; break; }
  if (!doc.tempoMap.empty()) {
    splitTempoRampAt(next, start);
    if (glideAtEnd) splitTempoRampAt(next, end - 1);
  }
  for (auto* list : automationLists(next))
    for (auto& a : *list) {
      std::vector<AutomationPoint> copies;
      for (auto& p : a.points) {
        if (p.tick >= end) p.tick += len;                                        // later music moves on
        else if (p.tick >= start) { AutomationPoint c = p; c.tick += len; copies.push_back(c); }  // the section's curve is copied
      }
      // The original keeps its value just before the copy (the point at end - 1); the copy starts
      // with the section's start value.
      a.points.insert(a.points.end(), copies.begin(), copies.end());
    }
  sortAutomation(next);
  for (auto& t : next.tracks) {
    std::vector<Clip> copies;
    for (auto& c : t.clips) {
      if (c.startTick >= end) c.startTick += len;
      else if (c.startTick >= start) { Clip copy = c; copy.startTick += len; copies.push_back(copy); }
    }
    if (t.clips.size() + copies.size() > 256) throw DocumentError("clips", "a track would pass 256 clips");
    for (auto& c : copies) { c.id = nextId(t.clips, "c"); t.clips.push_back(c); }
    sortClips(t);
    std::vector<AudioClip> audioCopies;
    for (auto& c : t.audioClips) {
      if (c.startTick >= end) c.startTick += len;
      else if (c.startTick >= start) { AudioClip copy = c; copy.startTick += len; audioCopies.push_back(copy); }
    }
    if (t.audioClips.size() + audioCopies.size() > 1024) throw DocumentError("audioClips", "a track would pass 1024 audio clips");
    for (auto& c : audioCopies) { c.id = nextId(t.audioClips, "x"); t.audioClips.push_back(c); }
  }
  for (auto& s : next.sections) if (s.startTick >= end) s.startTick += len;
  // Match the inserted time (BUG-SWEEP-22): the section's curve is copied self-contained, its effective
  // start value pinned at the copy's first tick; later changes follow the later music, and the follower
  // keeps the effective start value the music had at `end`.
  if (!doc.tempoMap.empty()) {
    std::vector<TempoPoint> tempo;
    bool copyStartPinned = false;
    for (const auto& p : next.tempoMap) {
      if (p.tick < end) tempo.push_back(p);  // the original music keeps its curve (the boundary splits above)
      if (p.tick >= start && p.tick < end) {
        TempoPoint c = p;
        c.tick += len;
        if (p.tick == start) { c.ramp = false; copyStartPinned = true; }  // the copy starts at a definite value
        tempo.push_back(c);
      }
    }
    if (!copyStartPinned) tempo.push_back({end, vStart, json::object(), false});
    const auto taken = [&](int64_t tick) { for (const auto& p : tempo) if (p.tick == tick) return true; return false; };
    if (glideAtEnd && len > 1 && !taken(end + len - 1))
      tempo.push_back({end + len - 1, vEndMinus, json::object(), true});  // the copy reproduces the glide the section cut
    for (const auto& p : next.tempoMap) if (p.tick >= end) tempo.push_back({p.tick + len, p.bpm, p.extra, p.ramp});
    if (glideAtEnd && end < doc.lengthTicks && !taken(end + len))
      tempo.push_back({end + len, vEnd, json::object(), false});  // the follower's effective start
    if (tempo.size() > (size_t)kMaxTempoPoints)
      throw DocumentError("tempoMap", "at most " + std::to_string(kMaxTempoPoints) + " tempo changes");
    std::sort(tempo.begin(), tempo.end(), [](const TempoPoint& a, const TempoPoint& b) { return a.tick < b.tick; });
    next.tempoMap = std::move(tempo);
  }
  if (next.loop.startTick >= end) { next.loop.startTick += len; next.loop.endTick += len; }
  next.lengthTicks += len;
  Section copy{nextId(next.sections, "s"), nameWithSuffix(original.name, " 2", kMaxSectionNameBytes), end, len, json::object(), original.color};
  next.sections.push_back(copy);
  sortSections(next);
  doc = std::move(next);
  return copy.id;
}

void insertTime(Document& doc, int64_t at, int64_t ticks) {
  if (at < 0 || at > doc.lengthTicks || ticks <= 0) throw DocumentError("ticks", "invalid place or length");
  if (doc.lengthTicks + ticks > 4096LL * 4 * kPpq) throw DocumentError("lengthTicks", "the song would be too long");
  // Tempo (BUG-SWEEP-24): the inserted empty bars hold the tempo playing at the cut; the music before
  // keeps its exact curve, and the music after keeps its own, re-anchored at the end of the inserted time
  // (the mirror of deleteTime's glide preservation, audit B03). Built before anything moves: a map over
  // the cap throws with the song untouched.
  std::vector<TempoPoint> tempo;
  if (!doc.tempoMap.empty()) {
    const double vAt = bpmAt(doc, at);
    const double vAtMinus = at >= 1 ? bpmAt(doc, at - 1) : doc.bpm;
    const bool valueJumps = at >= 1 && std::abs(vAt - vAtMinus) > 1e-9;
    bool glideAtCut = false, glideCrosses = false;
    for (size_t i = 0; i < doc.tempoMap.size(); ++i) {
      const TempoPoint& p = doc.tempoMap[i];
      if (p.tick == at) { glideAtCut = p.ramp; break; }
      if (p.tick > at) {
        const int64_t origin = i ? doc.tempoMap[i - 1].tick : 0;
        glideAtCut = p.ramp && origin < at;
        glideCrosses = p.ramp && (origin < at || at == 0);  // at the start, the glide leaves doc.bpm
        break;
      }
    }
    for (const auto& p : doc.tempoMap) if (p.tick < at) tempo.push_back(p);
    // A glide arriving at or crossing the cut keeps its shape up to the tick before it (deleteTime's trick).
    if (glideAtCut && at >= 2 && (tempo.empty() || at - 1 > tempo.back().tick))
      tempo.push_back({at - 1, vAtMinus, json::object(), true});
    if (valueJumps || glideAtCut) tempo.push_back({at, vAt, json::object(), false});  // the hold starts here
    bool reanchored = false;
    for (const auto& p : doc.tempoMap) {
      if (p.tick < at) continue;
      TempoPoint q = p;
      q.tick += ticks;
      if (p.tick == at) { q.ramp = false; reanchored = true; }  // its arrival glide stays before the cut
      tempo.push_back(q);
    }
    if (glideCrosses && !reanchored) tempo.push_back({at + ticks, vAt, json::object(), false});
    if (tempo.size() > (size_t)kMaxTempoPoints)
      throw DocumentError("tempoMap", "at most " + std::to_string(kMaxTempoPoints) + " tempo changes");
    std::sort(tempo.begin(), tempo.end(), [](const TempoPoint& a, const TempoPoint& b) { return a.tick < b.tick; });
  }
  splitClipsAt(doc, at);
  splitAutomationAt(doc, at);
  for (auto* list : automationLists(doc))
    for (auto& a : *list)
      for (auto& p : a.points) if (p.tick >= at) p.tick += ticks;
  for (auto& t : doc.tracks) {
    for (auto& c : t.clips) if (c.startTick >= at) c.startTick += ticks;
    for (auto& c : t.audioClips) if (c.startTick >= at) c.startTick += ticks;
    sortClips(t);
  }
  for (auto& sec : doc.sections) {
    if (sec.startTick >= at) sec.startTick += ticks;
    else if (sec.startTick + sec.lengthTicks > at) sec.lengthTicks += ticks;
  }
  doc.tempoMap = std::move(tempo);
  if (doc.loop.startTick >= at) doc.loop.startTick += ticks;
  if (doc.loop.endTick > at) doc.loop.endTick += ticks;
  doc.lengthTicks += ticks;
}

void deleteTime(Document& doc, int64_t from, int64_t ticks) {
  const int64_t to = from + ticks;
  if (from < 0 || ticks <= 0 || to > doc.lengthTicks) throw DocumentError("ticks", "invalid place or length");
  if (doc.lengthTicks - ticks < kPpq) throw DocumentError("lengthTicks", "the song would be empty");
  splitClipsAt(doc, from);
  splitClipsAt(doc, to);
  splitAutomationAt(doc, from);
  splitAutomationAt(doc, to);
  auto map = [&](int64_t t) { return t < from ? t : t < to ? from : t - ticks; };
  for (auto* list : automationLists(doc))
    for (auto& a : *list) {
      a.points.erase(std::remove_if(a.points.begin(), a.points.end(), [&](const AutomationPoint& p) { return p.tick > from && p.tick < to; }),
                     a.points.end());
      for (auto& p : a.points) p.tick = map(p.tick);
      // Two points may now share `from` (the one before the cut and the one after): keep the later value.
      for (size_t i = 1; i < a.points.size();)
        if (a.points[i].tick == a.points[i - 1].tick && a.points[i].tick == from && i >= 1) a.points.erase(a.points.begin() + (long)(i - 1));
        else ++i;
    }
  sortAutomation(doc);
  for (auto& t : doc.tracks) {
    t.clips.erase(std::remove_if(t.clips.begin(), t.clips.end(), [&](const Clip& c) { return c.startTick >= from && c.startTick < to; }), t.clips.end());
    for (auto& c : t.clips) if (c.startTick >= to) c.startTick -= ticks;
    t.audioClips.erase(std::remove_if(t.audioClips.begin(), t.audioClips.end(), [&](const AudioClip& c) { return c.startTick >= from && c.startTick < to; }),
                       t.audioClips.end());
    for (auto& c : t.audioClips) if (c.startTick >= to) c.startTick -= ticks;
    sortClips(t);
  }
  std::vector<Section> kept;
  for (auto sec : doc.sections) {
    const int64_t a = map(sec.startTick), b = map(sec.startTick + sec.lengthTicks);
    if (b <= a) continue;
    sec.startTick = a;
    sec.lengthTicks = b - a;
    kept.push_back(sec);
  }
  doc.sections = kept;
  // Tempo: the song plays as before up to `from`, and from there on as it played from `to` (audit B03: a glide
  // that crosses either edge keeps its shape, rather than the cut dropping its end point).
  if (!doc.tempoMap.empty()) {
    const int64_t oldLength = doc.lengthTicks;
    const double atTo = bpmAt(doc, to);
    const TempoPoint* firstAfter = nullptr;
    for (const auto& p : doc.tempoMap) if (p.tick > to) { firstAfter = &p; break; }
    const TempoPoint* crossing = nullptr;  // the point the song is heading for at `from`
    for (const auto& p : doc.tempoMap) if (p.tick > from) { crossing = &p; break; }
    std::vector<TempoPoint> tempo;
    for (const auto& p : doc.tempoMap) if (p.tick <= from) tempo.push_back(p);
    if (from == 0) {
      doc.bpm = atTo;
    } else {
      const bool step = to < oldLength && (std::abs(bpmAt(doc, from) - atTo) > 1e-9 || (firstAfter && firstAfter->ramp));
      if (crossing && crossing->ramp) {  // still gliding at `from`: arrive where the glide was
        const int64_t at = step || to >= oldLength ? from - 1 : from;
        if (at >= 1 && (tempo.empty() || at > tempo.back().tick)) {
          TempoPoint p = *crossing;
          p.tick = at;
          p.bpm = bpmAt(doc, at);
          tempo.push_back(p);
        }
      }
      // A point already at `from` arriving by a glide moves one tick back, so the step can take its place (or, at
      // the new end of the song, so it stays inside it).
      if ((step || to >= oldLength) && !tempo.empty() && tempo.back().tick == from) {
        TempoPoint arrive = tempo.back();
        tempo.pop_back();
        if (arrive.ramp && from - 1 >= 1 && (tempo.empty() || from - 1 > tempo.back().tick)) {
          arrive.tick = from - 1;
          arrive.bpm = bpmAt(doc, from - 1);
          tempo.push_back(arrive);
        }
      }
      if (step) {
        TempoPoint p;
        p.tick = from;
        p.bpm = atTo;
        tempo.push_back(p);
      }
    }
    for (auto p : doc.tempoMap)
      if (p.tick > to) { p.tick -= ticks; tempo.push_back(p); }
    doc.lengthTicks -= ticks;
    tempo.erase(std::remove_if(tempo.begin(), tempo.end(), [&](const TempoPoint& p) { return p.tick < 1 || p.tick >= doc.lengthTicks; }),
                tempo.end());
    // The inverse edit (insertTime then deleteTime) can land a hold pin and the moved point on the same
    // tick: keep the later value, as the automation lists do above.
    for (size_t i = 1; i < tempo.size();)
      if (tempo[i].tick == tempo[i - 1].tick) tempo.erase(tempo.begin() + (long)(i - 1));
      else ++i;
    doc.tempoMap = tempo;
  } else {
    doc.lengthTicks -= ticks;
  }
  const int64_t ls = map(doc.loop.startTick), le = map(doc.loop.endTick);
  doc.loop.startTick = ls;
  doc.loop.endTick = le;
  if (le <= ls) doc.loop.enabled = false;
}

// ---- effects


std::string effectName(const json& fx) { return fx.is_object() && fx.contains("name") && fx["name"].is_string() ? fx["name"].get<std::string>() : std::string(); }
bool effectBypassed(const json& fx) { return fx.is_object() && fx.contains("bypass") && fx["bypass"].is_boolean() && fx["bypass"].get<bool>(); }
double effectParam(const json& fx, const std::string& param, double fallback) {
  if (!fx.is_object() || !fx.contains("params") || !fx["params"].is_object()) return fallback;
  const json& p = fx["params"];
  return p.contains(param) && p[param].is_number() ? p[param].get<double>() : fallback;
}

json& effectChain(Document& doc, const std::string& trackId) { return trackId.empty() ? doc.masterEffects : trackById(doc, trackId).effects; }

const json& effectChain(const Document& doc, const std::string& trackId) {
  if (trackId.empty()) return doc.masterEffects;
  const Track* t = doc.findTrack(trackId);
  if (!t) throw DocumentError("tracks", "no track '" + trackId + "'");
  return t->effects;
}

std::vector<EffectMenuEntry> effectMenuEntries(const json& chain) {
  std::vector<EffectMenuEntry> out;
  for (int i = 0; i < (int)chain.size() && i < kMaxEffectsPerChain; ++i) {
    const json& fx = chain[(size_t)i];
    if (!fx.is_object()) continue;
    const bool plugin = fx.value("type", std::string()) == "plugin" && fx.contains("plugin");
    std::string label = plugin ? fx["plugin"].value("name", std::string("Plugin")) : effectName(fx);
    if (label.empty()) label = "Effect";
    out.push_back({i, plugin, plugin, std::move(label)});
  }
  return out;
}

namespace {
json& effectAt(Document& doc, const std::string& trackId, int index) {
  json& chain = effectChain(doc, trackId);
  if (index < 0 || index >= (int)chain.size()) throw DocumentError("effects", "no effect at " + std::to_string(index));
  return chain[(size_t)index];
}
}  // namespace

namespace {
// A new effect id in a chain: fx1, fx2... (never reused while the others are there).
std::string nextEffectId(const json& chain) {
  long highest = 0;
  for (const auto& fx : chain)
    if (fx.is_object() && fx.contains("id") && fx["id"].is_string() && fx["id"].get<std::string>().rfind("fx", 0) == 0)
      highest = std::max(highest, idNumber(fx["id"].get<std::string>().c_str() + 2));
  return "fx" + std::to_string(highest + 1);
}

// What a document keeps of a scanned plugin (the state is added when the engine gives it).
json pluginRef(const json& plugin) {
  if (!plugin.is_object() || !plugin.contains("id") || !plugin["id"].is_string() || !plugin.contains("file"))
    throw DocumentError("plugin", "a plugin needs its id and its file");
  std::string format = plugin.value("format", std::string("vst3"));
  for (auto& c : format) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  json ref = {{"format", format}, {"id", plugin["id"]}, {"name", plugin.value("name", "Plugin")},
              {"vendor", plugin.value("vendor", "")}, {"version", plugin.value("version", "")}, {"file", plugin["file"]}};
  if (plugin.contains("state") && plugin["state"].is_string()) ref["state"] = plugin["state"];
  return ref;
}

}  // namespace

std::string addPluginTrack(Document& doc, const json& plugin) {
  const json ref = pluginRef(plugin);
  if (doc.tracks.size() >= 128) throw DocumentError("tracks", "has the maximum of 128 tracks");
  Track t;
  t.id = uniqueTrackId(doc, "vst");
  t.name = ref["name"].get<std::string>().substr(0, 80);
  t.instrument.kind = "plugin";
  t.instrument.name.clear();
  t.instrument.plugin = ref;
  Pattern p;  // one 1-bar pattern over the whole song, as for the other instruments
  p.id = "p1";
  p.name = "1";
  p.lengthTicks = 4 * kPpq;
  t.patterns.push_back(p);
  t.clips.push_back({"c1", "p1", 0, doc.lengthTicks, 0, json::object()});
  doc.tracks.push_back(std::move(t));
  return doc.tracks.back().id;
}

int addPluginEffect(Document& doc, const std::string& trackId, const json& plugin) {
  const json ref = pluginRef(plugin);
  json& chain = effectChain(doc, trackId);
  if ((int)chain.size() >= kMaxEffectsPerChain) throw DocumentError("effects", "the chain has the maximum of 16 effects");
  chain.push_back({{"id", nextEffectId(chain)}, {"type", "plugin"}, {"plugin", ref}, {"bypass", false}});
  return (int)chain.size() - 1;
}

void replaceInstrumentWithPlugin(Document& doc, const std::string& trackId, const json& plugin) {
  Track& t = trackById(doc, trackId);
  if (!playsPatterns(t)) throw DocumentError("instrument", "an audio track or a bus has no instrument");
  Instrument next;
  next.kind = "plugin";
  next.plugin = pluginRef(plugin);
  if (t.instrument.kind == "plugin" && t.instrument.plugin.value("id", std::string()) == next.plugin.value("id", std::string()) &&
      t.instrument.plugin.value("file", std::string()) == next.plugin.value("file", std::string()))
    return;  // the same plugin: its state and automation stay as they are
  trackById(doc, trackId).instrument = next;
  Track& t2 = trackById(doc, trackId);
  // The old instrument's own curves and movements go; the mixer's and the effects' stay.
  auto own = [](const std::string& p) { return p != "gainDb" && p != "pan" && p.rfind("fx:", 0) != 0; };
  t2.automation.erase(std::remove_if(t2.automation.begin(), t2.automation.end(), [&](const Automation& a) { return own(a.param); }), t2.automation.end());
}

std::string pluginKey(const std::string& trackId, const std::string& effectId) {
  if (effectId.empty()) return "track:" + trackId;
  return "fx:" + (trackId.empty() ? std::string("master") : trackId) + ":" + effectId;
}

bool hasPlugins(const Document& doc) {
  auto chainHas = [](const json& chain) {
    for (const auto& fx : chain) if (fx.is_object() && fx.value("type", "") == "plugin") return true;
    return false;
  };
  if (chainHas(doc.masterEffects)) return true;
  for (const auto& t : doc.tracks) if (t.instrument.kind == "plugin" || chainHas(t.effects)) return true;
  return false;
}

std::vector<std::string> pluginKeys(const Document& doc) {
  std::vector<std::string> keys;
  auto chain = [&](const json& list, const std::string& trackId) {
    for (const auto& fx : list)
      if (fx.is_object() && fx.value("type", "") == "plugin" && fx.contains("id") && fx["id"].is_string() && fx.contains("plugin"))
        keys.push_back(pluginKey(trackId, fx["id"].get<std::string>()));
  };
  chain(doc.masterEffects, "");
  for (const auto& t : doc.tracks) {
    if (t.instrument.kind == "plugin") keys.push_back(pluginKey(t.id));
    chain(t.effects, t.id);
  }
  return keys;
}

json pluginStatesFor(const Document& doc, const std::vector<std::string>& keys) {
  const std::set<std::string> wanted(keys.begin(), keys.end());
  json out = json::object();
  auto take = [&](const json& plugin, const std::string& key) {
    if (!wanted.count(key) || !plugin.is_object()) return;
    const std::string state = plugin.value("state", "");
    if (!state.empty()) out[key] = state;
  };
  auto chain = [&](const json& list, const std::string& trackId) {
    for (const auto& fx : list)
      if (fx.is_object() && fx.value("type", "") == "plugin" && fx.contains("id") && fx["id"].is_string() && fx.contains("plugin"))
        take(fx["plugin"], pluginKey(trackId, fx["id"].get<std::string>()));
  };
  chain(doc.masterEffects, "");
  for (const auto& t : doc.tracks) {
    if (t.instrument.kind == "plugin") take(t.instrument.plugin, pluginKey(t.id));
    chain(t.effects, t.id);
  }
  return out;
}

int applyPluginStates(Document& doc, const json& states, std::vector<std::string>* changedKeys) {
  if (!states.is_object()) return 0;
  int changed = 0;
  auto set = [&](json& plugin, const std::string& key) {
    if (!states.contains(key) || !states[key].is_string() || !plugin.is_object()) return;
    if (plugin.value("state", "") == states[key].get<std::string>()) return;
    plugin["state"] = states[key];
    ++changed;
    if (changedKeys) changedKeys->push_back(key);
  };
  auto chain = [&](json& list, const std::string& trackId) {
    for (auto& fx : list)
      if (fx.is_object() && fx.value("type", "") == "plugin" && fx.contains("id") && fx["id"].is_string() && fx.contains("plugin"))
        set(fx["plugin"], pluginKey(trackId, fx["id"].get<std::string>()));
  };
  chain(doc.masterEffects, "");
  for (auto& t : doc.tracks) {
    if (t.instrument.kind == "plugin") set(t.instrument.plugin, pluginKey(t.id));
    chain(t.effects, t.id);
  }
  return changed;
}

std::vector<std::string> droppedPluginKeys(const Document& before, const Document& after) {
  // key -> which plugin it is (file and id; the state and the name are not what makes it another plugin)
  auto plugins = [](const Document& doc) {
    std::map<std::string, std::string> found;
    auto add = [&](const std::string& key, const json& plugin) {
      if (plugin.is_object()) found[key] = plugin.value("file", json()).dump() + "|" + plugin.value("id", json()).dump();
    };
    auto chain = [&](const json& list, const std::string& trackId) {
      for (const auto& fx : list)
        if (fx.is_object() && fx.value("type", "") == "plugin" && fx.contains("id") && fx["id"].is_string() && fx.contains("plugin"))
          add(pluginKey(trackId, fx["id"].get<std::string>()), fx["plugin"]);
    };
    chain(doc.masterEffects, "");
    for (const auto& t : doc.tracks) {
      if (t.instrument.kind == "plugin") add(pluginKey(t.id), t.instrument.plugin);
      chain(t.effects, t.id);
    }
    return found;
  };
  const auto was = plugins(before), now = plugins(after);
  std::vector<std::string> dropped;
  for (const auto& [key, plugin] : was) {
    const auto it = now.find(key);
    if (it == now.end() || it->second != plugin) dropped.push_back(key);
  }
  return dropped;
}

int keepLivePluginStates(const Document& from, Document& to) {
  auto identity = [](const json& plugin) { return plugin.value("file", json()).dump() + "|" + plugin.value("id", json()).dump(); };
  auto isPluginEffect = [](const json& fx) {
    return fx.is_object() && fx.value("type", "") == "plugin" && fx.contains("id") && fx["id"].is_string() && fx.contains("plugin");
  };
  // The plugins of the state being left, by key.
  std::map<std::string, const json*> left;
  auto leftChain = [&](const json& list, const std::string& trackId) {
    for (const auto& fx : list)
      if (isPluginEffect(fx) && fx["plugin"].is_object()) left[pluginKey(trackId, fx["id"].get<std::string>())] = &fx["plugin"];
  };
  leftChain(from.masterEffects, "");
  for (const auto& t : from.tracks) {
    if (t.instrument.kind == "plugin" && t.instrument.plugin.is_object()) left[pluginKey(t.id)] = &t.instrument.plugin;
    leftChain(t.effects, t.id);
  }
  int changed = 0;
  auto keep = [&](json& plugin, const std::string& key) {
    const auto it = left.find(key);
    if (!plugin.is_object() || it == left.end() || identity(*it->second) != identity(plugin)) return;
    bool moved = false;
    for (const char* field : {"state", "stateId"}) {
      const auto newest = it->second->find(field);
      if (newest == it->second->end() || (plugin.contains(field) && plugin[field] == *newest)) continue;
      plugin[field] = *newest;
      moved = true;
    }
    if (moved) ++changed;
  };
  auto chain = [&](json& list, const std::string& trackId) {
    for (auto& fx : list)
      if (isPluginEffect(fx)) keep(fx["plugin"], pluginKey(trackId, fx["id"].get<std::string>()));
  };
  chain(to.masterEffects, "");
  for (auto& t : to.tracks) {
    if (t.instrument.kind == "plugin") keep(t.instrument.plugin, pluginKey(t.id));
    chain(t.effects, t.id);
  }
  return changed;
}


int songRoot(const Document& doc) {
  // The bass tracks say the key; without one, every melodic track (drums never do).
  auto lower = [](std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
  };
  auto isDrums = [&](const Track& t) {
    const std::string n = lower(t.id + " " + t.name);
    return n.find("kick") != std::string::npos || n.find("hat") != std::string::npos ||
           n.find("snare") != std::string::npos || n.find("drum") != std::string::npos ||
           n.find("clap") != std::string::npos || n.find("perc") != std::string::npos;
  };
  auto isBass = [&](const Track& t) {
    return lower(t.name).find("bass") != std::string::npos;
  };
  for (int pass = 0; pass < 2; ++pass) {
    double weight[12] = {};
    double total = 0;
    for (const auto& t : doc.tracks) {
      if (isDrums(t) || t.instrument.kind == "audio" || t.instrument.kind == "bus") continue;
      if (pass == 0 && !isBass(t)) continue;
      for (const auto& p : t.patterns) {
        const int uses = std::max(1, t.clipsUsing(p.id));
        for (const auto& n : p.notes) {
          const double w = (double)std::max<int64_t>(1, n.length) * uses;
          weight[((n.pitch % 12) + 12) % 12] += w;
          total += w;
        }
      }
    }
    if (total > 0) return (int)(std::max_element(weight, weight + 12) - weight);
  }
  return -1;
}

int followSongKey(const Document& doc, int note) {
  const int root = songRoot(doc);
  if (root < 0) return note;
  int d = (root - note % 12 + 12) % 12;  // up to the root...
  if (d > 6) d -= 12;                      // ...or down, whichever is nearer
  return note + d;
}



namespace {
std::vector<Automation>& automationOf(Document& doc, const std::string& trackId) {
  return trackId.empty() ? doc.masterAutomation : trackById(doc, trackId).automation;
}
}  // namespace



void removeEffect(Document& doc, const std::string& trackId, int index) {
  const json fx = effectAt(doc, trackId, index);
  json& chain = effectChain(doc, trackId);
  chain.erase(chain.begin() + index);
  // Its automation goes with it.
  if (fx.contains("id") && fx["id"].is_string()) {
    const std::string prefix = "fx:" + fx["id"].get<std::string>() + ":";
    auto& list = automationOf(doc, trackId);
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Automation& a) { return a.param.rfind(prefix, 0) == 0; }), list.end());
  }
}

void moveEffect(Document& doc, const std::string& trackId, int from, int to) {
  json moved = effectAt(doc, trackId, from);
  json& chain = effectChain(doc, trackId);
  to = std::clamp(to, 0, (int)chain.size() - 1);
  chain.erase(chain.begin() + from);
  chain.insert(chain.begin() + to, moved);
}


void setEffectBypass(Document& doc, const std::string& trackId, int index, bool bypass) { effectAt(doc, trackId, index)["bypass"] = bypass; }

// ---- automation

namespace {
const std::vector<Automation>* automationOf(const Document& doc, const std::string& trackId) {
  if (trackId.empty()) return &doc.masterAutomation;
  const Track* t = doc.findTrack(trackId);
  return t ? &t->automation : nullptr;
}
// The plugin a 'vst:' parameter belongs to (the track's instrument, or a plugin effect of the chain),
// or null: its name for the window, and whether the parameter can be automated there at all.
const json* pluginOfParam(const Document& doc, const std::string& trackId, const std::string& param) {
  const Track* t = trackId.empty() ? nullptr : doc.findTrack(trackId);
  if (!trackId.empty() && !t) return nullptr;
  if (t && param.rfind("vst:", 0) == 0 && param.size() > 4 && t->instrument.kind == "plugin") return &t->instrument.plugin;
  if (param.rfind("fx:", 0) != 0) return nullptr;
  const size_t colon = param.find(':', 3);
  if (colon == std::string::npos || param.compare(colon + 1, 4, "vst:") != 0 || param.size() <= colon + 5) return nullptr;
  const std::string id = param.substr(3, colon - 3);
  for (const auto& fx : t ? t->effects : doc.masterEffects)
    if (fx.value("type", std::string()) == "plugin" && fx.value("id", std::string()) == id && fx.contains("plugin")) return &fx["plugin"];
  return nullptr;
}
AutomatableParam paramInfo(const Document& doc, const std::string& trackId, const std::string& param) {
  for (const auto& p : automatableParams(doc, trackId)) if (p.param == param) return p;
  if (pluginOfParam(doc, trackId, param)) return {param, param, 0, 1, 0.5, 0.5, "", false};  // a plugin parameter, 0..1
  throw DocumentError("automation", "'" + param + "' cannot be automated here");
}
Automation& automationAt(Document& doc, const std::string& trackId, const std::string& param) {
  for (auto& a : automationOf(doc, trackId)) if (a.param == param) return a;
  throw DocumentError("automation", "no automation of '" + param + "'");
}
}  // namespace

std::vector<AutomatableParam> automatableParams(const Document& doc, const std::string& trackId) {
  std::vector<AutomatableParam> out;
  const Track* t = trackId.empty() ? nullptr : doc.findTrack(trackId);
  if (!trackId.empty() && !t) return out;
  out.push_back({"gainDb", t ? "Volume" : "Master volume", -60, 12, 0, t ? t->mix.gainDb : doc.masterGainDb, "dB", false});
  if (t) out.push_back({"pan", "Pan", -1, 1, 0, t->mix.pan, "", false});
  // Automation already there (a plugin parameter): shown with the range of its own points, so it can be
  // seen, edited and cleared. Plugin parameters without a curve yet are offered by the engine/window.
  for (const auto& a : t ? t->automation : doc.masterAutomation) {
    if (std::any_of(out.begin(), out.end(), [&](const AutomatableParam& p) { return p.param == a.param; }) || a.points.empty()) continue;
    if (const json* plugin = pluginOfParam(doc, trackId, a.param)) {  // its name comes from the engine (the window asks)
      out.push_back({a.param, plugin->value("name", std::string("Plugin")) + " · " + a.param, 0, 1, 0.5, a.points[0].value, "", false});
      continue;
    }
    double lo = a.points[0].value, hi = lo;
    for (const auto& p : a.points) { lo = std::min(lo, p.value); hi = std::max(hi, p.value); }
    if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
    out.push_back({a.param, a.param + " (instrument)", lo, hi, lo, a.points[0].value, "", false});
  }
  return out;
}

std::string effectParamKey(const std::string& effectId, const std::string& param) { return "fx:" + effectId + ":" + param; }

const Automation* findAutomation(const Document& doc, const std::string& trackId, const std::string& param) {
  if (const auto* list = automationOf(doc, trackId))
    for (const auto& a : *list) if (a.param == param) return &a;
  return nullptr;
}

double automationValueAt(const Automation& a, int64_t tick) {
  if (a.points.empty()) return 0;
  if (tick <= a.points.front().tick) return a.points.front().value;
  if (tick >= a.points.back().tick) return a.points.back().value;
  auto it = std::upper_bound(a.points.begin(), a.points.end(), tick, [](int64_t v, const AutomationPoint& p) { return v < p.tick; });
  const AutomationPoint& b = *it;
  const AutomationPoint& prev = *(it - 1);
  return prev.value + (b.value - prev.value) * double(tick - prev.tick) / double(std::max<int64_t>(1, b.tick - prev.tick));
}

int setAutomationPoint(Document& doc, const std::string& trackId, const std::string& param, int64_t tick, double value) {
  const AutomatableParam info = paramInfo(doc, trackId, param);
  auto& list = automationOf(doc, trackId);
  Automation* a = nullptr;
  for (auto& x : list) if (x.param == param) { a = &x; break; }  // the first, as findAutomation
  if (!a) {
    if (list.size() >= 256) throw DocumentError("automation", "the maximum of 256 automations is reached");
    list.push_back(Automation{param, {}, json::object()});
    a = &list.back();
  }
  if (a->points.size() >= 100000) throw DocumentError("automation", "too many points");
  tick = std::clamp<int64_t>(tick, 0, doc.lengthTicks);
  const AutomationPoint p{tick, std::clamp(value, info.min, info.max), json::object()};
  auto it = std::lower_bound(a->points.begin(), a->points.end(), tick, [](const AutomationPoint& x, int64_t v) { return x.tick < v; });
  if (it != a->points.end() && it->tick == tick) { it->value = p.value; return (int)(it - a->points.begin()); }
  // Two statements: insert may reallocate, so begin() must be read after it.
  const auto inserted = a->points.insert(it, p);
  return (int)(inserted - a->points.begin());
}

int moveAutomationPoint(Document& doc, const std::string& trackId, const std::string& param, int index, int64_t tick, double value) {
  const AutomatableParam info = paramInfo(doc, trackId, param);
  Automation& a = automationAt(doc, trackId, param);
  if (index < 0 || index >= (int)a.points.size()) throw DocumentError("automation", "no point " + std::to_string(index));
  const int64_t lo = index > 0 ? a.points[(size_t)index - 1].tick + 1 : 0;
  const int64_t hi = index + 1 < (int)a.points.size() ? a.points[(size_t)index + 1].tick - 1 : doc.lengthTicks;
  if (lo <= hi) a.points[(size_t)index].tick = std::clamp(tick, lo, hi);  // between its neighbours (no room: it keeps its tick)
  a.points[(size_t)index].value = std::clamp(value, info.min, info.max);
  return index;
}

void removeAutomationPoint(Document& doc, const std::string& trackId, const std::string& param, int index) {
  Automation& a = automationAt(doc, trackId, param);
  if (index < 0 || index >= (int)a.points.size()) throw DocumentError("automation", "no point " + std::to_string(index));
  a.points.erase(a.points.begin() + index);
  if (a.points.empty()) clearAutomation(doc, trackId, param);
}

void clearAutomation(Document& doc, const std::string& trackId, const std::string& param) {
  auto& list = automationOf(doc, trackId);
  list.erase(std::remove_if(list.begin(), list.end(), [&](const Automation& a) { return a.param == param; }), list.end());
}

void setAutomationInRange(Document& doc, const std::string& trackId, const std::string& param, int64_t startTick, int64_t endTick,
                          const std::vector<AutomationPoint>& points, double heldValue) {
  const AutomatableParam info = paramInfo(doc, trackId, param);
  if (startTick < 0 || endTick <= startTick || endTick > doc.lengthTicks) throw DocumentError("automation", "the range must be inside the song and end after it starts");
  for (const auto& p : points)
    if (p.tick < startTick || p.tick >= endTick) throw DocumentError("automation", "a point at tick " + std::to_string(p.tick) + " is outside the range");
  const Automation* old = findAutomation(doc, trackId, param);
  auto valueAt = [&](int64_t tick) { return old && !old->points.empty() ? automationValueAt(*old, tick) : std::clamp(heldValue, info.min, info.max); };
  std::vector<AutomationPoint> kept;
  if (old) for (const auto& p : old->points) if (p.tick < startTick || p.tick > endTick) kept.push_back(p);
  std::vector<AutomationPoint> result;
  for (const auto& p : kept) if (p.tick < startTick) result.push_back(p);
  // Left edge: the old value right where the region begins (one tick earlier when a new point sits on the start).
  const bool pointOnStart = std::any_of(points.begin(), points.end(), [&](const AutomationPoint& p) { return p.tick == startTick; });
  const int64_t leftEdge = pointOnStart ? startTick - 1 : startTick;
  if (leftEdge >= 0 && (result.empty() || result.back().tick < leftEdge)) result.push_back({leftEdge, valueAt(leftEdge), json::object()});
  for (auto p : points) { p.value = std::clamp(p.value, info.min, info.max); p.extra = json::object(); result.push_back(p); }
  std::stable_sort(result.begin() + (long)(result.size() - points.size()), result.end(),
                   [](const AutomationPoint& x, const AutomationPoint& y) { return x.tick < y.tick; });
  // Right edge: the old value at the end of the region, from where the curve goes on as before.
  result.push_back({endTick, valueAt(endTick), json::object()});
  for (const auto& p : kept) if (p.tick > endTick) result.push_back(p);
  for (size_t i = 1; i < result.size(); ++i)
    if (result[i].tick == result[i - 1].tick) throw DocumentError("automation", "two points at tick " + std::to_string(result[i].tick));
  if (result.size() > 100000) throw DocumentError("automation", "too many points");
  auto& list = automationOf(doc, trackId);
  for (auto& a : list)
    if (a.param == param) { a.points = std::move(result); return; }
  if (list.size() >= 256) throw DocumentError("automation", "the maximum of 256 automations is reached");
  list.push_back(Automation{param, std::move(result), json::object()});
}

// ---- audio

namespace {

AudioClip& audioClipById(Track& t, const std::string& id) {
  for (auto& c : t.audioClips) if (c.id == id) return c;
  throw DocumentError("audioClips", "no audio clip '" + id + "'");
}
}  // namespace

double secondsPerTick(const Document& doc) { return 60.0 / (doc.bpm * kPpq); }

// The same sums as the engine's tempo map (core/tempo_map.cpp): steps are tick * seconds-per-tick, a ramp
// (the BPM in a straight line from the previous point to a point with ramp) is integrated in closed form.
namespace {
constexpr double kSecondsAtOneBpm = 60.0 / kPpq;  // seconds per tick at 1 BPM
// Seconds over d ticks of a segment that starts at bpm and changes by slope BPM per tick.
double segmentSeconds(double bpm, double slope, double d) {
  if (slope == 0 || d <= 0) return d * kSecondsAtOneBpm / bpm;
  return (kSecondsAtOneBpm / slope) * std::log1p(slope * d / bpm);
}
double segmentTicks(double bpm, double slope, double seconds) {  // the inverse
  if (slope == 0 || seconds <= 0) return seconds * bpm / kSecondsAtOneBpm;
  return (bpm / slope) * std::expm1(slope * seconds / kSecondsAtOneBpm);
}
// The slope (BPM per tick) of the segment from (fromTick, fromBpm) to point `next`.
double slopeTo(double fromTick, double fromBpm, const TempoPoint* next) {
  if (!next || !next->ramp || next->bpm == fromBpm) return 0;
  return (next->bpm - fromBpm) / ((double)next->tick - fromTick);
}
}  // namespace

double secondsAt(const Document& doc, double tick) {
  double from = 0, at = 0, bpm = doc.bpm;
  for (size_t i = 0;; ++i) {
    const TempoPoint* next = i < doc.tempoMap.size() ? &doc.tempoMap[i] : nullptr;
    const double slope = slopeTo(from, bpm, next);
    if (!next || next->tick > tick) return at + segmentSeconds(bpm, slope, tick - from);
    at += segmentSeconds(bpm, slope, (double)next->tick - from);
    from = (double)next->tick;
    bpm = next->bpm;
  }
}

double tickAtSeconds(const Document& doc, double seconds) {
  double from = 0, at = 0, bpm = doc.bpm;
  for (size_t i = 0;; ++i) {
    const TempoPoint* next = i < doc.tempoMap.size() ? &doc.tempoMap[i] : nullptr;
    const double slope = slopeTo(from, bpm, next);
    if (next) {
      const double end = at + segmentSeconds(bpm, slope, (double)next->tick - from);
      if (end <= seconds) { at = end; from = (double)next->tick; bpm = next->bpm; continue; }
    }
    return from + segmentTicks(bpm, slope, seconds - at);
  }
}

double secondsBetween(const Document& doc, double fromTick, double toTick) {
  if (doc.tempoMap.empty()) return (toTick - fromTick) * secondsPerTick(doc);
  return secondsAt(doc, toTick) - secondsAt(doc, fromTick);
}

std::string addAudioTrack(Document& doc, const std::string& name) {
  if (doc.tracks.size() >= 128) throw DocumentError("tracks", "has the maximum of 128 tracks");
  Track t;
  t.id = uniqueTrackId(doc, name.empty() ? "audio" : name);
  t.name = name.empty() ? "Audio" : cutUtf8(name, 80);
  t.instrument.kind = "audio";
  t.instrument.name.clear();
  doc.tracks.push_back(std::move(t));
  return doc.tracks.back().id;
}

std::string addAudioClip(Document& doc, const std::string& trackId, const std::string& assetId, int64_t startTick) {
  const Asset* asset = doc.findAsset(assetId);
  if (!asset) throw DocumentError("assetId", "no asset '" + assetId + "'");
  Track& t = trackById(doc, trackId);
  if (t.instrument.kind != "audio") throw DocumentError("tracks", "audio clips go on an audio track");
  if (t.audioClips.size() >= 1024) throw DocumentError("audioClips", "the track has the maximum of 1024 audio clips");
  AudioClip c;
  c.id = nextId(t.audioClips, "x");
  c.assetId = assetId;
  c.startTick = std::max<int64_t>(0, startTick);
  c.durationSeconds = std::max(kMinClipSeconds, asset->seconds());
  t.audioClips.push_back(c);
  return c.id;
}

void moveAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t startTick) {
  audioClipById(trackById(doc, trackId), clipId).startTick = std::max<int64_t>(0, startTick);
}

void trimAudioClipStart(Document& doc, const std::string& trackId, const std::string& clipId, int64_t startTick) {
  AudioClip& c = audioClipById(trackById(doc, trackId), clipId);
  // Song seconds of a tick: tick * spt with one tempo (as before the map), the map otherwise.
  const double spt = secondsPerTick(doc);
  auto sec = [&](double tick) { return doc.tempoMap.empty() ? tick * spt : secondsAt(doc, tick); };
  const double end = sec((double)c.startTick) + c.durationSeconds;   // where it ends, in song seconds
  const double fileStart = sec((double)c.startTick) - c.offsetSeconds;  // where the file's first sample sits
  const double firstTick = doc.tempoMap.empty() ? fileStart / spt : tickAtSeconds(doc, fileStart);
  int64_t s = std::max<int64_t>({0, startTick, (int64_t)std::ceil(firstTick - 1e-9)});  // not before the file
  while (s > 0 && end - sec((double)s) < kMinClipSeconds) --s;       // not past its own end (whole ticks)
  if (end - sec((double)s) < kMinClipSeconds) return;
  c.offsetSeconds = std::max(0.0, sec((double)s) - fileStart);
  c.startTick = s;
  c.durationSeconds = end - sec((double)s);
}

void trimAudioClipEnd(Document& doc, const std::string& trackId, const std::string& clipId, double durationSeconds) {
  AudioClip& c = audioClipById(trackById(doc, trackId), clipId);
  const Asset* asset = doc.findAsset(c.assetId);
  const double available = asset ? asset->seconds() - c.offsetSeconds : durationSeconds;
  c.durationSeconds = std::clamp(durationSeconds, kMinClipSeconds, std::max(kMinClipSeconds, available));
}

std::string splitAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t atTick) {
  Track& t = trackById(doc, trackId);
  AudioClip& c = audioClipById(t, clipId);
  // BUG-SWEEP-21 (owner ratified 2026-11-18): «split em clip congelado = recusa com mensagem + oferta de unfreeze
  // (um Undo); nunca meio-congelado.» A frozen clip holds the piece of the file from offsetSeconds for its whole
  // length (clip_grains.hpp); the split's offset re-anchor (offsetSeconds += cut below) would make the second
  // piece hold a DIFFERENT piece — a half-frozen state. Refuse with the unfreeze offer; unfreezeAudioClip (one
  // Undo step in the app) then split plays the file sample for sample (engine_tests null proof).
  if (c.freezeSeconds > 0)
    throw DocumentError("audioClips", "the clip is frozen: splitting it is refused (the second piece would hold a different part of the file). Unfreeze it first (one Undo), then split.");
  const double cut = secondsBetween(doc, (double)c.startTick, (double)atTick);
  if (cut < kMinClipSeconds || c.durationSeconds - cut < kMinClipSeconds) throw DocumentError("audioClips", "the split point must be inside the clip");
  if (t.audioClips.size() >= 1024) throw DocumentError("audioClips", "the track has the maximum of 1024 audio clips");
  AudioClip second = c;
  second.id = nextId(t.audioClips, "x");
  second.startTick = atTick;
  second.offsetSeconds = c.offsetSeconds + cut;
  second.durationSeconds = c.durationSeconds - cut;
  second.fadeInSeconds = 0;
  second.fadeOutSeconds = std::min(second.fadeOutSeconds, second.durationSeconds);
  c.durationSeconds = cut;
  c.fadeOutSeconds = 0;
  c.fadeInSeconds = std::min(c.fadeInSeconds, c.durationSeconds);  // a fade longer than the piece ends with it
  t.audioClips.push_back(second);  // c may move: take the id from the copy
  return second.id;
}

void unfreezeAudioClip(Document& doc, const std::string& trackId, const std::string& clipId) {
  // BUG-SWEEP-21: the unfreeze the split refusal offers. The clip plays its file normally again (the held piece
  // is let go); one Undo step in the app brings the freeze back. A plain clip is unchanged (no step).
  audioClipById(trackById(doc, trackId), clipId).freezeSeconds = 0;
}

std::string duplicateAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t startTick) {
  Track& t = trackById(doc, trackId);
  if (t.audioClips.size() >= 1024) throw DocumentError("audioClips", "the track has the maximum of 1024 audio clips");
  AudioClip copy = audioClipById(t, clipId);
  copy.id = nextId(t.audioClips, "x");
  copy.startTick = std::max<int64_t>(0, startTick);
  t.audioClips.push_back(copy);
  return copy.id;
}

std::string pasteAudioClip(Document& doc, const std::string& trackId, AudioClip copy, int64_t startTick) {
  Track& t = trackById(doc, trackId);
  if (t.instrument.kind != "audio") throw DocumentError("tracks", "audio clips go on an audio track");
  if (t.audioClips.size() >= 1024) throw DocumentError("audioClips", "the track has the maximum of 1024 audio clips");
  copy.id = nextId(t.audioClips, "x");
  copy.startTick = std::max<int64_t>(0, startTick);
  t.audioClips.push_back(copy);
  return copy.id;
}

std::string moveClipToTrack(Document& doc, const std::string& fromTrack, const std::string& clipId, const std::string& toTrack, int64_t startTick) {
  startTick = std::max<int64_t>(0, startTick);
  Track& from = trackById(doc, fromTrack);
  trackById(doc, toTrack);  // throws when it is not there
  for (const auto& c : from.audioClips) {
    if (c.id != clipId) continue;
    if (fromTrack == toTrack) { moveAudioClip(doc, fromTrack, clipId, startTick); return clipId; }
    Track& to = trackById(doc, toTrack);
    if (to.instrument.kind != "audio") throw DocumentError("tracks", "audio clips go on an audio track");
    if (to.audioClips.size() >= 1024) throw DocumentError("audioClips", "the track has the maximum of 1024 audio clips");
    AudioClip moved = c;
    moved.id = nextId(to.audioClips, "x");
    moved.startTick = startTick;
    to.audioClips.push_back(moved);
    removeAudioClip(doc, fromTrack, clipId);
    return moved.id;
  }
  const Clip clip = clipById(from, clipId);  // a copy: the tracks' vectors change below
  if (fromTrack == toTrack) { moveClip(from, clipId, startTick); return clipId; }
  Track& to = trackById(doc, toTrack);
  if (!playsPatterns(to)) throw DocumentError("tracks", "pattern clips go on an instrument track");
  if (to.clips.size() >= 256) throw DocumentError("clips", "the track has the maximum of 256 clips");
  if (to.patterns.size() >= 64) throw DocumentError("patterns", "the track has the maximum of 64 patterns");
  Pattern pattern = patternById(from, clip.patternId);
  pattern.id = nextId(to.patterns, "p");
  pattern.name = std::to_string(to.patterns.size() + 1);
  to.patterns.push_back(pattern);
  Clip moved = clip;
  moved.id = nextId(to.clips, "c");
  moved.patternId = pattern.id;
  moved.startTick = startTick;
  to.clips.push_back(moved);
  sortClips(to);
  removeClip(trackById(doc, fromTrack), clipId);
  return moved.id;
}

void removeAudioClip(Document& doc, const std::string& trackId, const std::string& clipId) {
  Track& t = trackById(doc, trackId);
  audioClipById(t, clipId);
  t.audioClips.erase(std::remove_if(t.audioClips.begin(), t.audioClips.end(), [&](const AudioClip& c) { return c.id == clipId; }), t.audioClips.end());
}

int64_t audioClipEndTick(const Document& doc, const AudioClip& c) {
  if (doc.tempoMap.empty()) return c.startTick + (int64_t)std::llround(c.durationSeconds / secondsPerTick(doc));
  return (int64_t)std::llround(tickAtSeconds(doc, secondsAt(doc, (double)c.startTick) + c.durationSeconds));
}

void cropAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t fromTick, int64_t toTick) {
  {
    const AudioClip& c = audioClipById(trackById(doc, trackId), clipId);
    fromTick = std::max(fromTick, c.startTick);  // only ever shorter: trimmed-away audio stays away
    toTick = std::min(toTick, audioClipEndTick(doc, c));
  }
  if (toTick <= fromTick) throw DocumentError("audioClips", "the selection is empty");
  const int64_t oldStart = audioClipById(trackById(doc, trackId), clipId).startTick;
  const int64_t oldEnd = audioClipEndTick(doc, audioClipById(trackById(doc, trackId), clipId));
  trimAudioClipStart(doc, trackId, clipId, fromTick);
  AudioClip& c = audioClipById(trackById(doc, trackId), clipId);
  trimAudioClipEnd(doc, trackId, clipId, secondsBetween(doc, (double)c.startTick, (double)toTick));
  if (c.startTick != oldStart) c.fadeInSeconds = 0;  // a new cut: its old fade belonged to the old edge
  if (audioClipEndTick(doc, c) != oldEnd) c.fadeOutSeconds = 0;
  c.fadeInSeconds = std::min(c.fadeInSeconds, c.durationSeconds);
  c.fadeOutSeconds = std::min(c.fadeOutSeconds, c.durationSeconds);
}

int sliceAudioClip(Document& doc, const std::string& trackId, const std::string& clipId, int64_t stepTicks) {
  if (stepTicks <= 0) throw DocumentError("audioClips", "the slice step must be positive");
  const AudioClip& c = audioClipById(trackById(doc, trackId), clipId);
  const int64_t start = c.startTick, end = audioClipEndTick(doc, c);
  std::vector<int64_t> cuts;
  for (int64_t t = (start / stepTicks + 1) * stepTicks; t < end; t += stepTicks) {
    const double cut = secondsBetween(doc, (double)start, (double)t);
    if (cut >= kMinClipSeconds && c.durationSeconds - cut >= kMinClipSeconds) cuts.push_back(t);
  }
  if (trackById(doc, trackId).audioClips.size() + cuts.size() > 1024) throw DocumentError("audioClips", "the track would pass 1024 audio clips");
  std::string piece = clipId;
  for (int64_t t : cuts) piece = splitAudioClip(doc, trackId, piece, t);  // each cut splits the last piece
  return (int)cuts.size() + 1;
}

void removeAudioRange(Document& doc, const std::string& trackId, const std::string& clipId, int64_t fromTick, int64_t toTick) {
  AudioClip& c = audioClipById(trackById(doc, trackId), clipId);
  const int64_t start = c.startTick, end = audioClipEndTick(doc, c);
  fromTick = std::max(fromTick, start);
  toTick = std::min(toTick, end);
  if (toTick <= fromTick) throw DocumentError("audioClips", "the selection is empty");
  const bool keepsStart = secondsBetween(doc, (double)start, (double)fromTick) >= kMinClipSeconds,
             keepsEnd = secondsBetween(doc, (double)toTick, (double)end) >= kMinClipSeconds;
  if (!keepsStart && !keepsEnd) { removeAudioClip(doc, trackId, clipId); return; }
  if (!keepsStart) {
    trimAudioClipStart(doc, trackId, clipId, toTick);
    AudioClip& k = audioClipById(trackById(doc, trackId), clipId);
    k.fadeInSeconds = 0;  // the start is a new cut
    k.fadeOutSeconds = std::min(k.fadeOutSeconds, k.durationSeconds);
    return;
  }
  if (!keepsEnd) {
    trimAudioClipEnd(doc, trackId, clipId, secondsBetween(doc, (double)start, (double)fromTick));
    AudioClip& k = audioClipById(trackById(doc, trackId), clipId);
    k.fadeOutSeconds = 0;  // the end is a new cut
    k.fadeInSeconds = std::min(k.fadeInSeconds, k.durationSeconds);
    return;
  }
  const std::string second = splitAudioClip(doc, trackId, clipId, fromTick);  // c may move: no use after this
  trimAudioClipStart(doc, trackId, second, toTick);
}

void setSongLength(Document& doc, int64_t lengthTicks) {
  if (lengthTicks < kPpq || lengthTicks > 4096LL * 4 * kPpq) throw DocumentError("lengthTicks", "invalid song length");
  // Shortening keeps the documented removal of changes past the new end, but a glide the retained music
  // was riding gets an in-range endpoint at the new last tick (BUG-SWEEP-25), the same arrival trick
  // deleteTime uses when it cuts to the end of the song. Without it the retained passage flattens.
  if (lengthTicks < doc.lengthTicks && !doc.tempoMap.empty()) {
    const TempoPoint* crossing = nullptr;
    for (const auto& p : doc.tempoMap) if (p.tick >= lengthTicks) { crossing = &p; break; }
    if (crossing && crossing->ramp) {
      const int64_t at = lengthTicks - 1;
      const TempoPoint* lastKept = nullptr;
      for (const auto& p : doc.tempoMap) if (p.tick < lengthTicks) lastKept = &p;
      if (at >= 1 && (!lastKept || at > lastKept->tick)) {
        TempoPoint p = *crossing;
        p.tick = at;
        p.bpm = bpmAt(doc, at);
        doc.tempoMap.push_back(p);  // stays sorted: after every kept point, before every removed one
      }
    }
  }
  doc.lengthTicks = lengthTicks;
  // Tempo changes after the new end go with it (they could never play).
  doc.tempoMap.erase(std::remove_if(doc.tempoMap.begin(), doc.tempoMap.end(), [&](const TempoPoint& t) { return t.tick >= lengthTicks; }), doc.tempoMap.end());
}

void setTempoMap(Document& doc, std::vector<TempoPoint> changes) {
  if (changes.size() > (size_t)kMaxTempoPoints) throw DocumentError("tempoMap", "at most " + std::to_string(kMaxTempoPoints) + " tempo changes");
  std::sort(changes.begin(), changes.end(), [](const TempoPoint& a, const TempoPoint& b) { return a.tick < b.tick; });
  for (size_t i = 0; i < changes.size(); ++i) {
    const std::string where = "tempoMap[" + std::to_string(i) + "]";
    if (changes[i].tick <= 0 || changes[i].tick >= doc.lengthTicks) throw DocumentError(where + ".tick", "must be after the start and inside the song");
    if (i && changes[i].tick == changes[i - 1].tick) throw DocumentError(where + ".tick", "two tempo changes on the same tick");
    if (!(changes[i].bpm >= 40 && changes[i].bpm <= 600)) throw DocumentError(where + ".bpm", "must be 40-600");
  }
  doc.tempoMap = std::move(changes);
}

double bpmAt(const Document& doc, int64_t tick) {
  double from = 0, bpm = doc.bpm;
  for (const auto& t : doc.tempoMap) {
    if (t.tick <= tick) { from = (double)t.tick; bpm = t.bpm; continue; }
    if (t.ramp) return bpm + (t.bpm - bpm) * ((double)tick - from) / ((double)t.tick - from);  // gliding to t
    break;
  }
  return bpm;
}

void removeTrack(Document& doc, const std::string& trackId) {
  const auto it = std::find_if(doc.tracks.begin(), doc.tracks.end(), [&](const Track& t) { return t.id == trackId; });
  if (it == doc.tracks.end()) throw DocumentError("tracks", "no track '" + trackId + "'");
  const std::string goesTo = it->mix.output;
  doc.tracks.erase(it);
  for (auto& t : doc.tracks) {  // what fed it goes where it went; sends to it end
    if (t.mix.output == trackId) t.mix.output = goesTo;
    t.mix.sends.erase(std::remove_if(t.mix.sends.begin(), t.mix.sends.end(), [&](const Send& s) { return s.to == trackId; }), t.mix.sends.end());
    for (auto& fx : t.effects)  // a compressor it keyed goes back to its own input
      if (fx.is_object() && fx.value("sidechain", "") == trackId) fx.erase("sidechain");
  }
}

std::string addBusTrack(Document& doc, const std::string& name) {
  if (doc.tracks.size() >= 128) throw DocumentError("tracks", "has the maximum of 128 tracks");
  Track t;
  t.id = uniqueTrackId(doc, name.empty() ? "bus" : name);
  t.name = name.empty() ? "Bus" : cutUtf8(name, 80);
  t.instrument.kind = "bus";
  t.instrument.name.clear();
  for (const char* c : kGroupColors) {  // a group colour no other bus of the song has (the first one if all are used)
    bool used = false;
    for (const Track& o : doc.tracks) used = used || (o.instrument.kind == "bus" && o.color == c);
    if (!used) { t.color = c; break; }
  }
  if (t.color.empty()) t.color = kGroupColors[0];
  doc.tracks.push_back(std::move(t));
  return doc.tracks.back().id;
}

namespace {
// Applies a routing change to a copy and keeps it only if the routing stays valid.
template <typename Change>
void reroute(Document& doc, const std::string& trackId, Change&& change) {
  Track& t = trackById(doc, trackId);
  const Mix before = t.mix;
  change(t.mix);
  try { checkRouting(doc); } catch (...) { t.mix = before; throw; }
}
Send& sendById(Track& t, const std::string& sendId) {
  for (auto& s : t.mix.sends) if (s.id == sendId) return s;
  throw DocumentError("sends", "no send '" + sendId + "' on track '" + t.id + "'");
}
}  // namespace

void setTrackOutput(Document& doc, const std::string& trackId, const std::string& busId) {
  reroute(doc, trackId, [&](Mix& m) { m.output = busId == "master" ? "" : busId; });
}

std::string addSend(Document& doc, const std::string& trackId, const std::string& busId, double gainDb, bool pre) {
  std::string id;
  reroute(doc, trackId, [&](Mix& m) {
    if (m.sends.size() >= (size_t)kMaxSends) throw DocumentError("sends", "the track has the maximum of " + std::to_string(kMaxSends) + " sends");
    for (int n = 1; id.empty(); ++n) {
      const std::string candidate = "s" + std::to_string(n);
      if (std::none_of(m.sends.begin(), m.sends.end(), [&](const Send& s) { return s.id == candidate; })) id = candidate;
    }
    m.sends.push_back(Send{id, busId, std::clamp(gainDb, -60.0, 12.0), pre, json::object()});
  });
  return id;
}

void setSend(Document& doc, const std::string& trackId, const std::string& sendId, double gainDb, bool pre) {
  Send& s = sendById(trackById(doc, trackId), sendId);
  s.gainDb = std::clamp(gainDb, -60.0, 12.0);
  s.pre = pre;
}

void removeSend(Document& doc, const std::string& trackId, const std::string& sendId) {
  Track& t = trackById(doc, trackId);
  sendById(t, sendId);
  t.mix.sends.erase(std::remove_if(t.mix.sends.begin(), t.mix.sends.end(), [&](const Send& s) { return s.id == sendId; }), t.mix.sends.end());
}

} // namespace core::doc
