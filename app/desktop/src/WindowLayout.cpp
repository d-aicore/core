// The detachable windows' layout model — see WindowLayout.h.
#include "WindowLayout.h"
#include <algorithm>
#include <cstdlib>
#include <string>

namespace core::app::winlayout {
namespace {

const char* stateName(State s) {
  switch (s) {
    case State::Detached: return "detached";
    case State::Hidden: return "hidden";
    default: return "docked";
  }
}

State stateOf(const std::string& s) {
  if (s == "detached") return State::Detached;
  if (s == "hidden") return State::Hidden;
  return State::Docked;
}

// A settings field read without ever throwing: a wrong type (a string where a number belongs) is as unreadable
// as a missing one, and comes back as the default.
int intOf(const nlohmann::json& j, const char* key, int fallback) {
  const auto it = j.find(key);
  return it != j.end() && it->is_number_integer() ? it->get<int>() : fallback;
}

bool boolOf(const nlohmann::json& j, const char* key, bool fallback) {
  const auto it = j.find(key);
  return it != j.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

std::string stringOf(const nlohmann::json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

struct Area { int x = 0, y = 0, w = 0, h = 0; };

Area intersection(const Rect& r, const Rect& s) {
  const int x = std::max(r.x, s.x), y = std::max(r.y, s.y);
  const int right = std::min(r.x + r.w, s.x + s.w), bottom = std::min(r.y + r.h, s.y + s.h);
  return {x, y, std::max(0, right - x), std::max(0, bottom - y)};
}

long long areaOf(const Area& a) { return (long long)a.w * a.h; }

// The monitor a place belongs to: the one it overlaps most ("" index -1: none).
int bestScreen(const Rect& r, const std::vector<Rect>& screens) {
  int best = -1;
  long long bestArea = 0;
  for (size_t i = 0; i < screens.size(); ++i) {
    const long long a = areaOf(intersection(r, screens[i]));
    if (a > bestArea) { bestArea = a; best = (int)i; }
  }
  return best;
}

// A size no bigger than the monitor, never under 1 px.
Rect fitSize(const Rect& r, const Rect& screen) {
  Rect out = r;
  out.w = std::max(1, std::min(r.w, screen.w));
  out.h = std::max(1, std::min(r.h, screen.h));
  return out;
}

// A place moved (not resized) so the whole window sits inside the monitor.
Rect clampPos(const Rect& r, const Rect& screen) {
  Rect out = r;
  out.x = std::max(screen.x, std::min(r.x, screen.x + screen.w - r.w));
  out.y = std::max(screen.y, std::min(r.y, screen.y + screen.h - r.h));
  return out;
}

}  // namespace

nlohmann::json toJson(const Panel& p) {
  return {{"state", stateName(p.state)}, {"x", p.rect.x}, {"y", p.rect.y}, {"w", p.rect.w}, {"h", p.rect.h}, {"max", p.rect.maximized}};
}

Panel fromJson(const nlohmann::json& j) {
  Panel p;
  if (!j.is_object()) return p;
  p.state = stateOf(stringOf(j, "state"));
  p.rect.x = intOf(j, "x", 0);
  p.rect.y = intOf(j, "y", 0);
  p.rect.w = intOf(j, "w", 0);
  p.rect.h = intOf(j, "h", 0);
  p.rect.maximized = boolOf(j, "max", false);
  return p;
}

bool reachable(const Rect& r, const std::vector<Rect>& screens) {
  if (r.empty()) return false;
  // At least a strip of the title bar on some monitor: 60 x 32 px, or the whole window when it is smaller.
  const int needW = std::min(r.w, 60), needH = std::min(r.h, 32);
  for (const auto& s : screens) {
    const auto i = intersection(r, s);
    if (i.w >= needW && i.h >= needH) return true;
  }
  return false;
}

Rect validated(const Rect& r, const std::vector<Rect>& screens, const Rect& fallback) {
  if (screens.empty()) return fallback;
  if (r.empty()) return clampPos(fitSize(fallback, screens[0]), screens[0]);
  const int best = bestScreen(r, screens);
  if (best >= 0) {
    // On a monitor (fully or partly): keep the size when it fits, and move it no further than its edge.
    const Rect& screen = screens[(size_t)best];
    return clampPos(fitSize(r, screen), screen);
  }
  // Off every monitor: the fallback's monitor, the fallback's place, the window's own size when it fits.
  const int fb = bestScreen(fallback, screens);
  const Rect& screen = screens[(size_t)(fb >= 0 ? fb : 0)];
  Rect out = fitSize(r, screen);
  out.x = fallback.x;
  out.y = fallback.y;
  return clampPos(out, screen);
}

Rect bringBack(const Rect& r, const std::vector<Rect>& screens, int slot) {
  if (screens.empty()) return r;
  const Rect& primary = screens[0];
  Rect out = r.empty() ? Rect{0, 0, 960, 600} : r;
  out = fitSize(out, primary);
  const int step = 24 * std::max(0, slot);
  out.x = primary.x + step;
  out.y = primary.y + step;
  out.maximized = false;  // the command shows every window again, not its maximized state
  return clampPos(out, primary);
}

}  // namespace core::app::winlayout