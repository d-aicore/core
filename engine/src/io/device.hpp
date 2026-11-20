#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include "core/server.hpp"

namespace core {
// Audio output through miniaudio (WASAPI shared/exclusive on Windows; null backend for headless tests).
std::unique_ptr<AudioOutput> makeMiniaudioOutput();

// S7 (engine RT measurement, owner round-1 item 6): TEST-ONLY probe seam, null by default. Production
// cost is ONE relaxed atomic load per callback (the null check in MiniaudioOutput::callback); nothing
// else changes — no RT-path behaviour change, no default change (D51). When a measurement probe arms
// it, the audio callback publishes its in-callback window and its thread id through the probe's own
// state, may run the probe's directed fault (negative controls), and hands the probe the same per-block
// timing recordCallback gets. Justification: quality bar 3 asks for alloc/lock/IO proof by measurement,
// not by reading — the flag needs thread attribution so control-thread activity is never miscounted as
// audio-thread work, the inject point lets the proof FAIL on purpose (a plugin-DLL fault would not be
// seen by an exe-side probe: the DLL has its own CRT/imports), and the observer gives exact per-block
// max/p99 (the existing EngineStats loadBands are 1 %-of-budget bands — 13.3 us at 64 samples/48 kHz —
// too coarse for a p99 claim). One seam serves all three; engine.cpp stays untouched.
struct RtProbe {
  std::atomic<bool>* inCallback;        // probe-owned: true for the whole callback, false outside
  std::atomic<uint32_t>* audioThreadId; // probe-owned: the device thread's id, written each callback
  void (*inject)();                     // negative control run inside the window (null = none)
  void (*block)(uint64_t renderNs, uint64_t budgetNs, uint64_t gapNs, uint64_t frames);  // observer (null = none)
};
extern std::atomic<RtProbe*> rtProbe;  // defined in device.cpp; null unless a probe arms it
} // namespace core
