// Core plays plugins (Diogo 2026-11-16): its own instruments (kick, W-Core, hats, snare, drums, sampler, Core Glitch)
// left the engine. What is registered here are the track kinds with no built-in sound: a plugin (the graph plays the
// plugin; this silent one stands in while it is missing), an audio track, a bus, and a track with notes and no
// instrument yet ("none", also an old song's Core instrument, kept in the song, not played).
// The one exception is the user-sample host ("sample", BUG-SWEEP-10 positive, owner ratified 2026-11-18): it is
// not a sound source — it PLAYS the user's audio files (dsp/sample_host.hpp), the way an audio track plays its
// clips. The graph builds it with the zones' audio; the empty host here stands in for a caller without zones.
#include "instruments.hpp"
#include "registry.hpp"
#include "sample_host.hpp"

namespace core {

namespace {

class Silent final : public Instrument {
public:
  using Instrument::Instrument;
  const char* name() const override { return "plugin"; }
  void noteOn(int, double) override {}
  void noteOff(int) override {}
  void allNotesOff() override {}
  void render(float*, int) override {}
  void adopt(const Instrument&) override {}
};

}  // namespace

void registerNativeInstruments(NativeRegistry& reg) {
  // A placeholder spec keeps them "known" to validation; the graph gives them a silent instrument.
  const std::vector<ParamSpec> none{{"_", 0, 0, 0, ""}};
  const std::function<std::unique_ptr<Instrument>(double)> silent = [](double sr) { return std::make_unique<Silent>(sr); };
  for (const char* id : {"plugin", "audio", "bus", "none"}) reg.add({id, NativeKind::Instrument, 1, true, none, silent, nullptr});
  // The user-sample host: its playback controls (no oscillator, no filter — the file is the sound). The order
  // is the runtime params[] slot SampleHost reads. The graph builds the real host (it needs the zones' audio).
  const std::vector<ParamSpec> host = {
      {"level", 0, -24, 24, "dB"},   {"start", 0, 0, 60000, "ms"}, {"pitched", 0, 0, 1, ""},
      {"release", 30, 5, 2000, "ms"}, {"retrigger", 1, 0, 1, ""},  {"decay", 10000, 10, 10000, "ms"}};
  const std::function<std::unique_ptr<Instrument>(double)> emptyHost = [](double sr) {
    return std::make_unique<SampleHost>(sr, std::vector<SampleHostZone>{});
  };
  reg.add({"sample", NativeKind::Instrument, kSampleHostVersion, false, host, emptyHost, nullptr});
}

std::unique_ptr<Instrument> makeInstrument(const std::string& name, double sampleRate) {
  const NativePluginDesc* d = nativePlugins().find(NativeKind::Instrument, name);
  if (d && d->makeInstrument) return d->makeInstrument(sampleRate);
  return nullptr;
}

}  // namespace core
