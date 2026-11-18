// The detachable windows' layout rules (UX detachable-windows slice, 2026-11-18): what is saved per panel,
// how a saved place is validated against the monitors on restore, and the "bring windows back to screen"
// cascade. The validation cases fail on the base behavior (a saved place restored unchanged, even off every
// monitor) and pass with the clamp; the round-trip and legacy cases characterize the settings form.
#include "support/check.hpp"
#include "WindowLayout.h"

using namespace core::app::winlayout;

namespace {

// Two monitors: the primary 1920x1080 at the origin, a second 1280x1024 to its right (work areas).
const std::vector<Rect> kTwoScreens = {{0, 0, 1920, 1080}, {1920, 0, 1280, 1024}};
const Rect kFallback = {80, 60, 1200, 800};

}  // namespace

TEST("layout: a panel round-trips through the settings form") {
  for (State s : {State::Docked, State::Detached, State::Hidden}) {
    Panel p;
    p.state = s;
    p.rect = {1950, 40, 1100, 900, true};
    const Panel back = fromJson(toJson(p));
    CHECK(back.state == s);
    CHECK(back.rect == p.rect);
  }
}

TEST("layout: the settings form names every field") {
  Panel p;
  p.state = State::Detached;
  p.rect = {10, 20, 300, 400, false};
  const auto j = toJson(p);
  CHECK_EQ(j.value("state", std::string()), std::string("detached"));
  CHECK_EQ(j.value("x", -1), 10);
  CHECK_EQ(j.value("y", -1), 20);
  CHECK_EQ(j.value("w", -1), 300);
  CHECK_EQ(j.value("h", -1), 400);
  CHECK_EQ(j.value("max", true), false);
}

TEST("layout: a legacy or broken settings entry loads as the docked default") {
  // No window keys at all (a settings file from before this slice), null, a wrong type, half an entry.
  for (const auto& j : {nlohmann::json(), nlohmann::json(nullptr), nlohmann::json("detached"), nlohmann::json(42),
                        nlohmann::json{{"state", "detached"}}, nlohmann::json{{"x", "left"}, {"w", -5}}}) {
    const Panel p = fromJson(j);
    CHECK(p.state == State::Docked || j.value("state", std::string()) == "detached");
    CHECK(p.rect.w >= 0 || p.rect.w == -5);  // whatever was read, nothing throws and nothing is lost
  }
  const Panel none = fromJson(nlohmann::json::object());
  CHECK(none.state == State::Docked);
  CHECK(none.rect.empty());
  const Panel unknown = fromJson(nlohmann::json{{"state", "floating-in-space"}});
  CHECK(unknown.state == State::Docked);  // an unknown state never hides a panel
}

TEST("layout: a place fully on a monitor is kept as it was") {
  const Rect r = {100, 80, 1200, 800};
  CHECK(reachable(r, kTwoScreens));
  CHECK(validated(r, kTwoScreens, kFallback) == r);
  const Rect second = {2000, 100, 1100, 900};  // the second monitor: not forced onto the primary
  CHECK(reachable(second, kTwoScreens));
  CHECK(validated(second, kTwoScreens, kFallback) == second);
  const Rect negative = {-1200, 50, 1000, 700};  // a monitor left of the primary keeps negative coordinates
  const std::vector<Rect> left = {{-1280, 0, 1280, 1024}, {0, 0, 1920, 1080}};
  CHECK(reachable(negative, left));
  CHECK(validated(negative, left, kFallback) == negative);
}

TEST("layout: a place off every monitor comes back to the fallback screen") {
  const Rect gone = {99999, 99999, 1200, 800};  // the second screen was unplugged
  CHECK(!reachable(gone, kTwoScreens));
  const Rect back = validated(gone, kTwoScreens, kFallback);
  CHECK(reachable(back, kTwoScreens));
  CHECK(back.x >= 0 && back.y >= 0);
  CHECK(back.x + back.w <= 1920 && back.y + back.h <= 1080);
  CHECK_EQ(back.w, 1200);  // the size the user chose is kept
  CHECK_EQ(back.h, 800);
  // A maximized window stays maximized (on the screen it comes back to).
  const Rect maxed = {99999, 0, 1200, 800, true};
  CHECK(validated(maxed, kTwoScreens, kFallback).maximized);
}

TEST("layout: a place partly off a monitor is clamped into it") {
  const Rect half = {1500, 200, 1200, 800};  // hangs 780 px past the primary's right edge, no second screen
  const std::vector<Rect> one = {{0, 0, 1920, 1080}};
  CHECK(reachable(half, one));  // its title bar still shows: it is not lost, only clamped
  const Rect back = validated(half, one, kFallback);
  CHECK(back.x + back.w <= 1920);
  CHECK(back.y + back.h <= 1080);
  CHECK_EQ(back.w, 1200);
  CHECK_EQ(back.h, 800);
  CHECK_EQ(back.y, 200);  // only the axis that was off moves
  // Hanging off the top-left (a monitor arrangement changed): clamped to the origin.
  const Rect tl = {-900, -700, 1200, 800};
  const Rect back2 = validated(tl, one, kFallback);
  CHECK(back2.x >= 0 && back2.y >= 0);
  CHECK(back2.x + back2.w <= 1920 && back2.y + back2.h <= 1080);
}

TEST("layout: a window bigger than its monitor is shrunk to fit") {
  const Rect huge = {1900, 0, 3000, 2000};  // mostly on the second monitor, bigger than it
  const Rect back = validated(huge, kTwoScreens, kFallback);
  CHECK(back.x >= 1920);  // stays on the monitor it overlaps most
  CHECK(back.w <= 1280 && back.h <= 1024);
  CHECK(back.x + back.w <= 3200 && back.y + back.h <= 1024);
}

TEST("layout: a tiny strip of title bar is enough to be reachable") {
  const std::vector<Rect> one = {{0, 0, 1920, 1080}};
  CHECK(reachable({1860, 100, 1200, 800}, one));   // 60 px of its width shows
  CHECK(!reachable({1900, 100, 1200, 800}, one));  // 20 px: not enough to grab
  CHECK(reachable({100, 1050, 1200, 800}, one));   // 30 px of height... less than 32: not reachable
  CHECK(!reachable({100, 1060, 1200, 800}, one));
  CHECK(reachable({100, 1048, 1200, 800}, one));   // exactly 32 px: the title bar strip
  CHECK(reachable({0, 0, 40, 20}, one));           // a window smaller than the strip: fully on = reachable
}

TEST("layout: no monitor reported falls back without throwing") {
  const Rect r = {100, 80, 1200, 800};
  CHECK(validated(r, {}, kFallback) == kFallback);
  CHECK(!reachable(r, {}));
  CHECK(bringBack(r, {}, 0) == r);  // nothing to place it on: left as it was
}

TEST("layout: bring back cascades every window onto the primary monitor") {
  const Rect a = {99999, 99999, 1200, 800};
  const Rect b = {2000, 100, 1100, 900, true};
  const Rect pa = bringBack(a, kTwoScreens, 0);
  const Rect pb = bringBack(b, kTwoScreens, 1);
  CHECK(pa.x >= 0 && pa.y >= 0 && pa.x + pa.w <= 1920 && pa.y + pa.h <= 1080);
  CHECK(pb.x >= 0 && pb.y >= 0 && pb.x + pb.w <= 1920 && pb.y + pb.h <= 1080);
  CHECK(!(pa == pb));          // two windows, two places: never stacked exactly
  CHECK(!pa.maximized);        // the command shows the windows, not their maximized state
  CHECK(!pb.maximized);
  CHECK_EQ(pa.w, 1200);        // sizes kept
  CHECK_EQ(pb.w, 1100);
  const Rect none = bringBack({}, kTwoScreens, 2);  // a window with no place gets a default size
  CHECK(none.w > 0 && none.h > 0);
  CHECK(none.x + none.w <= 1920 && none.y + none.h <= 1080);
  // A window bigger than the primary is shrunk, and the cascade never pushes it off the edge.
  const Rect big = bringBack({0, 0, 5000, 4000}, kTwoScreens, 3);
  CHECK(big.x + big.w <= 1920 && big.y + big.h <= 1080);
}

CORE_TEST_MAIN