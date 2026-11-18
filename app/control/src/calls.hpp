// Core as programming (Diogo, 2026-11-05): every device of Core is a FUNCTION with a signature (its args are its
// knobs, with ranges), and a track's role (kick, bass, lead, pad...) is its TYPE. A preset is a saved call.
//
// This is all of the AI that lives in Core. Core stays focused on making music by hand; the AI (chat, model, guides,
// tastes, sample search) lives in AICORE. What Core gives it:
//   - the functions it can call now, with their signatures, and the song in a few lines (coreState, written to a
//     file by the app: control/bridge.hpp);
//   - checking a proposal of calls against the signatures and the song (validateProposal): unknown functions, knobs,
//     values out of range, missing sections are left out with the reason; types and styles that do not fit give a
//     warning, never a refusal (the style guides, it never blocks);
//   - applying it when Diogo presses Apply (applyProposal): the app commits it as ONE undo step, and nothing outside
//     the calls can change.
// A proposal: {"id": "...", "summary": "...", "calls": [{"fn": "stutter", "on": "Acid lead", "preset": "gate-16"}]}
//
// Core makes no sound of its own (Diogo, 2026-11-18): anything that generates or processes sound is a plugin. The
// functions today write notes and clips or open windows: "stutter" (a track's notes become repeats, by recipe),
// "voicestutter" (a track's audio clips become fragment clips, by recipe), "mix" (a track's volume and pan, the
// master's volume) and "open" (opens a track's effects, its plugin window, the patterns, the mixer or the score,
// after Apply). The song's plugins are reached through the role map and the song tools (doc::role_tools), not here.
#pragma once
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "document.hpp"
#include "samples.hpp"

namespace core::control {

using json = nlohmann::json;

constexpr int kCallsFormatVersion = 1;
constexpr const char* kAiLabelPrefix = "AI: ";  // the undo label of an applied proposal

// One argument of a function: a knob with its range ("roll" 0..1, "mix" 0..1, "cutoff" 20..20000 Hz).
struct FunctionArg {
  std::string id, name, hint, unit;
  double min = 0, max = 1, def = 0;
};

struct CoreFunction {
  std::string id;           // "stutter", "voicestutter", "mix", "open"
  std::string family;       // "stutter" | "voicestutter" | "mix" | "open"
  std::string name;         // as Core shows it
  std::string role;         // the kind of sound it is made for ("lead", "bass layer", "build-up"); "" = any
  std::string description;
  int version = 1;
  std::vector<std::string> styles;  // the styles it is made for (rack style keys); empty = any
  std::vector<std::string> tags;
  std::vector<FunctionArg> args;
  bool rises = false;       // acts over a section: "section" limits it to a part of the song
  bool available = true;
  std::string unavailable;  // why not
  std::vector<std::string> presets;  // a recipe by id ("preset" in a call)
  std::map<std::string, std::vector<std::string>> options;  // text choices: the allowed values
  json extra = json::object();       // written as is into the function's state (preset details)

  const FunctionArg* findArg(const std::string& id) const;
};

// The functions this Core can call now: STUTTER, VOICE STUTTER, the mixer and opening windows. Core makes no sound
// of its own; the plugins are reached through the role map and the song tools (doc::role_tools).
std::vector<CoreFunction> buildFunctions();
// By id, or by the part after the family ("buildup/roll", "reverb") when only one function has it.
const CoreFunction* findFunction(const std::vector<CoreFunction>& functions, const std::string& id);

// "Dark psycore", "hi-tech", "Full-on"... -> the rack style key ("darkpsycore", "hitech", "slambient", "psytrance");
// "" when it says nothing about a style.
std::string styleKey(const std::string& text);
// The song's style key (its template's style), "" when not known.
std::string songStyleKey(const doc::Document& song);

// The type of a track: lead, pad, vox, fx (from its name), else kick, bass, hat, openhat, accent, perc, impact,
// riser (as the sample library says); "" when nothing says.
std::string trackType(const doc::Track& track);
// Whether a function's role suits a track type (an unknown role or type always suits).
bool roleFits(const std::string& functionRole, const std::string& type);

// What the AI reads (core-state.json): {format: "core-state", version, functions: [{id, family, name, role,
// description, version, styles, tags, rises, available, unavailable, args: [{id, name, hint, unit, min,
// max, default}]}], song: {name, style, bpm, bars, sections: [{name, startBar, endBar}], tracks: [{id, name, type,
// sound, devices: [plugin names]}]}, samples: {count, roles: {role: n}} or null}. Never the library itself.
json functionsJson(const std::vector<CoreFunction>& functions);
json songJson(const doc::Document& song);
json coreState(const std::vector<CoreFunction>& functions, const doc::Document& song, const doc::SampleIndex* samples);

// ---- a proposal: calls, checked

struct Call {
  const CoreFunction* fn = nullptr;
  std::string trackId, trackName;  // "" = Diogo chooses when applying; "master" for the master (mix and open)
  std::map<std::string, double> args;  // checked: known knobs, in range
  std::string sectionName;             // a rise over this section (rising functions only)
  int64_t startTick = 0, endTick = 0;
  std::string preset;                  // a recipe's id ("stutter" and "voicestutter")
  std::string open;                    // "open": "effects", "instrument", "patterns", "mixer", "score" or an effect id
  std::map<std::string, std::string> texts;  // text choices, checked against the function's options
  std::vector<std::string> warnings;   // types that do not fit: kept, shown
};

struct Proposal {
  std::string id, summary;
  std::vector<Call> calls;
  std::vector<std::string> problems;  // why a call or a value was left out (shown, never applied)
  json raw;
  bool needsTrack() const;            // calls without a track (Diogo picks one when applying)
  std::vector<std::string> warnings() const;  // of every call
};

// Checks a proposal against the functions and the song: known functions this computer can run, known knobs in
// range, tracks and sections of the song. Types that do not fit give warnings. Never throws.
Proposal validateProposal(const json& raw, const std::vector<CoreFunction>& functions, const doc::Document& song,
                          const doc::SampleIndex* samples);
// One line per call, for the window: "Build-up: roll on Acid lead (roll 0.7, speed 0.5), rising over Build-up".
std::string describeCall(const Call& call);

// What applying needs from the app. The library is kept for the caller's convenience: no call writes a sample
// any more (Core makes no sound of its own; a sampler is a plugin).
struct ApplyEnv {
  const doc::SampleIndex* samples = nullptr;
};
// Makes every call of the proposal on `song` (the caller commits it as one undo step). chosenTrack: for the calls
// without a track ("master" for the master). Throws doc::DocumentError when a call is refused: then nothing
// should be committed. Returns one line per call, as applied.
std::vector<std::string> applyProposal(doc::Document& song, const Proposal& p, const std::string& chosenTrack, const ApplyEnv& env);

// What the window opens once the proposal is applied (the "open" calls): {track id ("master" for the master),
// "effects" | "instrument" | an effect id}. Opening changes nothing in the song.
struct OpenRequest { std::string trackId, what; };
std::vector<OpenRequest> opensOf(const Proposal& p, const std::string& chosenTrack);

}  // namespace core::control
