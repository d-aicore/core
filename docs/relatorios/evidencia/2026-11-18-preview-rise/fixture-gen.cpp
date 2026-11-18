// Null-render fixture generator (evidence tool for CLEAN-07 / BUG-LEGACY-RISE; not part of Core).
// Writes a deterministic stereo WAV and two core-project v2 fixtures:
//   null-audio.json  (F1): two audio tracks with gain/pan/fades/offsets — the live file-audio path.
//   null-plugin.json (F2): the self-test synth (a deterministic sine per note) playing three notes.
// Both are rendered by `core-engine render` before and after the removals; the WAV sha256 must not change.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "io/wav.hpp"
#include "json.hpp"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: fixture-gen <out-dir> <plugin-scan.json>\n");
    return 1;
  }
  const std::string dir = argv[1];
  std::ifstream pf(argv[2]);
  if (!pf) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
  const nlohmann::json found = nlohmann::json::parse(pf);
  if (!found.is_array() || found.empty()) { std::fprintf(stderr, "scan found no plugin\n"); return 1; }
  const std::string plugin = found[0].dump();

  const int sr = 48000, n = 96000;  // 2.0 s
  const double pi = 3.14159265358979323846;
  std::vector<float> L(n), R(n);
  for (int i = 0; i < n; ++i) {
    const double t = (double)i / sr;
    L[(size_t)i] = (float)(0.6 * std::sin(2 * pi * 220.0 * t) + 0.2 * std::sin(2 * pi * 1760.0 * t));
    R[(size_t)i] = (float)(0.5 * std::sin(2 * pi * 330.0 * t + 0.3));
  }
  const std::string wav = dir + "/tone.wav";
  core::writeWav(wav, L, R, sr, 24);

  std::ofstream(dir + "/null-audio.json")
      << "{\"format\":\"core-project\",\"version\":2,\"name\":\"null audio\",\"bpm\":120,"
         "\"lengthTicks\":7680,\"masterGainDb\":-1.5,\"tracks\":["
         "{\"id\":\"a1\",\"name\":\"Audio 1\",\"instrument\":\"audio\",\"gainDb\":-2.0,\"pan\":-0.25,"
         "\"audioClips\":[{\"path\":\"" << wav << "\",\"startTick\":0,\"offsetSeconds\":0.1,"
         "\"durationSeconds\":2.0,\"gainDb\":-1.0,\"fadeInSeconds\":0.25,\"fadeOutSeconds\":0.5,\"mute\":false}]},"
         "{\"id\":\"a2\",\"name\":\"Audio 2\",\"instrument\":\"audio\",\"gainDb\":0.0,\"pan\":0.3,"
         "\"audioClips\":[{\"path\":\"" << wav << "\",\"startTick\":3840,\"offsetSeconds\":0.5,"
         "\"durationSeconds\":1.5,\"gainDb\":0.0,\"fadeInSeconds\":0.0,\"fadeOutSeconds\":0.25,\"mute\":false}]}]}";

  std::ofstream(dir + "/null-plugin.json")
      << "{\"format\":\"core-project\",\"version\":2,\"name\":\"null plugin\",\"bpm\":120,"
         "\"lengthTicks\":7680,\"masterGainDb\":0,\"tracks\":["
         "{\"id\":\"synth\",\"name\":\"Synth\",\"instrument\":\"plugin\",\"plugin\":" << plugin << ","
         "\"patternTicks\":7680,\"notes\":["
         "{\"tick\":0,\"length\":960,\"pitch\":60,\"velocity\":0.8},"
         "{\"tick\":1920,\"length\":480,\"pitch\":67,\"velocity\":0.6},"
         "{\"tick\":3840,\"length\":1920,\"pitch\":64,\"velocity\":0.9}]}]}";
  std::printf("fixtures written to %s (tone.wav, null-audio.json, null-plugin.json)\n", dir.c_str());
  return 0;
}