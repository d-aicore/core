// OUT-OF-REPO timing probe plugins (STAGE 2 timing/latency proof). No repo file is touched.
// Two variants from one source, mirroring app/selftest-plugins/TestPlugins.cpp:
//   CORE_TIMING_DELAY  -> CoreTimingDelay: an effect that delays its input by a KNOWN amount and
//                        reports it as latency (setLatencySamples), as a lookahead limiter would.
//                        The delay is read from CORE_TIMING_DELAY_SAMPLES at construction (default 480)
//                        and can be moved at runtime through the "delay" parameter (0..1 -> 0..4095),
//                        which also re-reports latency, to exercise Server::checkPluginLatency.
//                        "gain" defaults to 1.0 (unity) so aligned renders subtract to silence.
//   CORE_TIMING_CLICK  -> CoreTimingClick: an instrument that emits a ONE-SAMPLE impulse at the exact
//                        note-on sample position (amplitude = velocity), then silence. Reports 0 latency.
//                        A sharp transient to pin MIDI onset samples in the render.
// Both are deterministic (no modulation, no randomness): the same input gives the same samples.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>

namespace {

int envInt(const char* name, int fallback) {
#ifdef _MSC_VER
  char* buf = nullptr; size_t len = 0;
  if (_dupenv_s(&buf, &len, name) == 0 && buf) { const int v = std::atoi(buf); free(buf); return v; }
  return fallback;
#else
  const char* v = std::getenv(name);
  return v ? std::atoi(v) : fallback;
#endif
}

class ProbeBase : public juce::AudioProcessor {
public:
  explicit ProbeBase(const BusesProperties& buses) : AudioProcessor(buses) {
    addParameter(gain_ = new juce::AudioParameterFloat(juce::ParameterID{"gain", 1}, "Gain", 0.0f, 1.0f, 1.0f));
  }
  const juce::String getName() const override { return JucePlugin_Name; }
  double getTailLengthSeconds() const override { return 0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}
  bool hasEditor() const override { return true; }
  juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
  void releaseResources() override {}
  void getStateInformation(juce::MemoryBlock& data) override { juce::MemoryOutputStream(data, false).writeFloat(gain_->get()); }
  void setStateInformation(const void* data, int size) override {
    if (size >= 4) gain_->setValueNotifyingHost(gain_->convertTo0to1(juce::MemoryInputStream(data, (size_t)size, false).readFloat()));
  }
protected:
  juce::AudioParameterFloat* gain_ = nullptr;
};

#if CORE_TIMING_DELAY
class TimingDelay final : public ProbeBase {
public:
  static constexpr int kMaxDelay = 4096;
  TimingDelay() : ProbeBase(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    int want = envInt("CORE_TIMING_DELAY_SAMPLES", 480);
    if (want < 0) want = 0;
    if (want >= kMaxDelay) want = kMaxDelay - 1;
    // Range 0..kMaxDelay-1 in SAMPLES (get() returns the delay directly, no normalization drift).
    addParameter(delayParam_ = new juce::AudioParameterFloat(juce::ParameterID{"delay", 1}, "Delay", 0.0f, (float)(kMaxDelay - 1), (float)want));
    delay_ = want;
    setLatencySamples(delay_);
  }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
  }
  void prepareToPlay(double, int) override { for (auto& c : line_) c.fill(0.0f); wp_ = 0; }
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
    // A moved "delay" parameter changes the actual delay AND re-reports it (the host polls latencySamples()).
    const int want = std::clamp((int)std::lround(delayParam_->get()), 0, kMaxDelay - 1);
    if (want != delay_) { delay_ = want; setLatencySamples(delay_); }
    const float g = gain_->get();
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      for (int c = 0; c < 2; ++c) {
        const int rp = (wp_ - delay_ + kMaxDelay) % kMaxDelay;
        const float out = line_[(size_t)c][(size_t)rp];
        line_[(size_t)c][(size_t)wp_] = buffer.getSample(c, i);
        buffer.setSample(c, i, out * g);
      }
      wp_ = (wp_ + 1) % kMaxDelay;
    }
  }
private:
  juce::AudioParameterFloat* delayParam_ = nullptr;
  std::array<std::array<float, kMaxDelay>, 2> line_{};
  int wp_ = 0;
  int delay_ = 480;
};
juce::AudioProcessor* make() { return new TimingDelay(); }
#endif

#if CORE_TIMING_CLICK
class TimingClick final : public ProbeBase {
public:
  TimingClick() : ProbeBase(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
  void prepareToPlay(double, int) override {}
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
    buffer.clear();
    for (const auto m : midi) {
      const auto msg = m.getMessage();
      if (msg.isNoteOn()) {
        const int at = std::clamp(m.samplePosition, 0, buffer.getNumSamples() - 1);
        const float amp = msg.getFloatVelocity() * gain_->get();
        buffer.addSample(0, at, amp);
        buffer.addSample(1, at, amp);
      }
    }
  }
};
juce::AudioProcessor* make() { return new TimingClick(); }
#endif

}  // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return make(); }
