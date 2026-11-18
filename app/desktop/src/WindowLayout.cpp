// The detachable windows' layout model — see WindowLayout.h.
#include "WindowLayout.h"

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

}  // namespace

nlohmann::json toJson(const Panel& p) {
  return {{"state", stateName(p.state)}, {"x", p.rect.x}, {"y", p.rect.y}, {"w", p.rect.w}, {"h", p.rect.h}, {"max", p.rect.maximized}};
}

Panel fromJson(const nlohmann::json& j) {
  Panel p;
  if (!j.is_object()) return p;
  p.state = stateOf(j.value("state", std::string("docked")));
  p.rect.x = j.value("x", 0);
  p.rect.y = j.value("y", 0);
  p.rect.w = j.value("w", 0);
  p.rect.h = j.value("h", 0);
  p.rect.maximized = j.value("max", false);
  return p;
}

bool reachable(const Rect& r, const std::vector<Rect>& screens) {
  // BASE (before the validation): no check at all — every saved place was taken as it was.
  (void)r;
  (void)screens;
  return true;
}

Rect validated(const Rect& r, const std::vector<Rect>& screens, const Rect& fallback) {
  // BASE (before the validation): the saved place restored unchanged, even off every monitor.
  (void)screens;
  (void)fallback;
  return r;
}

Rect bringBack(const Rect& r, const std::vector<Rect>& screens, int slot) {
  // BASE (before the command): nothing moved.
  (void)screens;
  (void)slot;
  return r;
}

}  // namespace core::app::winlayout