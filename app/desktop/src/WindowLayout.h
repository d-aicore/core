// The detachable windows' layout model (UX detachable-windows slice, 2026-11-18): what is saved per panel in
// the user settings and how a saved place is checked against the monitors before a window goes back there.
// Pure logic, no JUCE: the desktop shell turns juce::Rectangle / Displays into these rects and back, and the
// merge tests prove the rules without opening a window.
//
// Pulse's concepts, Core's own code: every panel is docked in the main window by default, can pop out into an
// OS window on any monitor, and its place is kept between sessions. A place no monitor shows anymore (the
// second screen was unplugged) never takes a window off screen: it is clamped back, and the "bring windows
// back to screen" command puts every detached window on the primary screen in a cascade.
#pragma once
#include "json.hpp"  // engine/third_party (core_domain's public include)
#include <vector>

namespace core::app::winlayout {

enum class State { Docked, Detached, Hidden };

// A place in screen coordinates (the work area of a monitor, or a window's bounds).
struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool maximized = false;
  bool operator==(const Rect& o) const = default;
  bool empty() const { return w <= 0 || h <= 0; }
};

struct Panel {
  State state = State::Docked;
  Rect rect;  // the detached window's place (kept while docked too, so detaching goes back where it was)
};

// The settings form: {"state":"docked|detached|hidden","x":..,"y":..,"w":..,"h":..,"max":bool}.
nlohmann::json toJson(const Panel& p);
// Tolerant of everything a settings file may hold: missing keys, an older form, wrong types, null. Anything
// unreadable comes back as the default (docked, no place) — never a window lost to a bad file.
Panel fromJson(const nlohmann::json& j);

// A window is reachable when at least a strip of its title bar shows on some monitor (60 x 32 px, or the whole
// window when it is smaller than that). The drag handle must be on screen, or the window cannot be moved back.
bool reachable(const Rect& r, const std::vector<Rect>& screens);

// The place a window goes to on restore:
//  - fully on a monitor: unchanged;
//  - partly off: clamped into the monitor it overlaps most (its size kept when it fits, shrunk when it does not);
//  - fully off (or no place saved): the fallback place, on the fallback's monitor, size kept when it fits.
// `screens` empty (no monitor reported): the fallback, unchanged. The maximized flag survives.
Rect validated(const Rect& r, const std::vector<Rect>& screens, const Rect& fallback);

// "Bring windows back to screen": slot `slot` of a cascade on the primary monitor (screens[0]), 24 px apart.
// The size is kept (shrunk to the monitor when bigger); a window with no place gets 960 x 600. Never maximized:
// the point of the command is to see every window again.
Rect bringBack(const Rect& r, const std::vector<Rect>& screens, int slot);

}  // namespace core::app::winlayout