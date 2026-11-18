#pragma once
#include <string>
#include "core/project.hpp"

namespace core {
json demoProject(double bpm);  // kick + rolling bass + hats notes, cutoff envelope, kick roll (8 bars); the own
                               // instruments load as "none" (2026-11-16), so it measures scheduling on silent tracks
// Runs the measurements and returns the report. outDir: where example WAVs go ("" = none).
json runMeasure(const std::string& outDir, const json& realtime);
} // namespace core
