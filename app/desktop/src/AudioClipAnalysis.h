// Audio-clip analysis for the editor (cut silence): pure buffer helpers that only measure audio — they make no
// sound and write no file. Kept independent of the removed sample-making module (Diogo 2026-11-18: Core makes no
// sound of its own; the DAW keeps what does not make sound).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace core::audio {

using Channels = std::vector<std::vector<float>>;

// The frames [from, to) where the sound is: from the first frame louder than thresholdDb (on any channel),
// minus startMarginMs (so the attack stays whole), to the last frame louder than it, plus endMarginMs (the tail
// fades on its own). to <= from when everything is under the threshold (silence).
struct AudibleRange { int64_t from = 0, to = 0; };

// True when any sample is NaN or Inf: a damaged file, not silence.
inline bool hasBrokenSamples(const Channels& channels) {
  for (const auto& ch : channels)
    for (float v : ch)
      if (!std::isfinite(v)) return true;
  return false;
}

inline AudibleRange audibleRange(const Channels& channels, double sampleRate, double thresholdDb = -60,
                                 double startMarginMs = 1, double endMarginMs = 20) {
  AudibleRange r;
  if (channels.empty() || !(sampleRate > 0)) return r;
  const int64_t length = (int64_t)channels[0].size();
  const float threshold = (float)std::pow(10.0, thresholdDb / 20.0);
  auto loud = [&](int64_t i) {
    for (const auto& ch : channels)
      if (i < (int64_t)ch.size() && std::abs(ch[(size_t)i]) > threshold) return true;
    return false;
  };
  int64_t first = 0;
  while (first < length && !loud(first)) ++first;
  if (first == length) return r;  // silence
  int64_t last = length - 1;
  while (last > first && !loud(last)) --last;
  r.from = std::max<int64_t>(0, first - std::llround(startMarginMs / 1000.0 * sampleRate));
  r.to = std::min<int64_t>(length, last + 1 + std::llround(endMarginMs / 1000.0 * sampleRate));
  return r;
}

}  // namespace core::audio