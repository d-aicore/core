#pragma once
#include <string>
#include "core/project.hpp"

namespace core {
json demoProject(double bpm);  // a generated-audio backing track (8 bars): Core has no built-in instruments
                               // (2026-11-16), so the demo sounds from a deterministic audio fixture, not from
                               // removed builtins (L10, 2026-11-18). The render loads it through an AssetStore.
// Runs the measurements and returns the report. outDir: where example WAVs go ("" = none).
json runMeasure(const std::string& outDir, const json& realtime);
} // namespace core
