// The test plugins (one file, built once per plugin): CORE_TEST_SYNTH (a sine per note), CORE_TEST_GAIN or
// CORE_TEST_DELAY (a delay that reports it as latency), CORE_TEST_THROW (a gain that throws after 0.1 s, audit 6.4),
// CORE_TEST_CRASH (an instrument that crashes after 0.1 s), CORE_TEST_FAULT (an effect that crashes on demand in a
// chosen entry point outside processBlock, or in a parameter getter). All have one parameter, "gain" (0..1, 0.5),
// saved in their state, and the generic editor. The synth's note 0 plays nothing: it tells the host that its state
// changed without moving a parameter, as a plugin does when a sample or a wavetable is loaded in its window.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace {

// True when the environment variable is set to a non-empty, non-"0" value (the fault plugins read it once per call).
bool envSet(const char* name) {
#ifdef _MSC_VER
  char* buf = nullptr;
  size_t len = 0;
  if (_dupenv_s(&buf, &len, name) == 0 && buf) { const bool set = buf[0] != '\0' && buf[0] != '0'; free(buf); return set; }
  return false;
#else
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
#endif
}

// An access violation on purpose (a write through a null pointer), as CORE_TEST_CRASH does: volatile so the
// compiler cannot prove the write invalid and drop it. The host catches it with structured exception handling.
void crashNow() {
  static volatile int* volatile nowhere = nullptr;
  *nowhere = 1;
}

class TestPlugin : public juce::AudioProcessor {
public:
  explicit TestPlugin(const BusesProperties& buses) : AudioProcessor(buses) {
    addParameter(gain_ = new juce::AudioParameterFloat(juce::ParameterID{"gain", 1}, "Gain", 0.0f, 1.0f, 0.5f));
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

#if CORE_TEST_SYNTH
// A sine per note: frequency from the pitch, level velocity x gain, 2 ms attack, 20 ms release.
class Synth final : public TestPlugin {
public:
  Synth() : TestPlugin(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
  void prepareToPlay(double sampleRate, int) override { rate_ = sampleRate; for (auto& v : voices_) v = {}; }
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
    buffer.clear();
    int pos = 0;
    auto render = [&](int to) {
      for (auto& v : voices_) {
        if (v.level <= 0 && !v.on) continue;
        const double step = 2 * juce::MathConstants<double>::pi * 440.0 * std::pow(2.0, (v.pitch - 69) / 12.0) / rate_;
        for (int i = pos; i < to; ++i) {
          const double target = v.on ? 1.0 : 0.0, rate = v.on ? 1.0 / (0.002 * rate_) : 1.0 / (0.02 * rate_);
          v.level = target > v.level ? std::min(target, v.level + rate) : std::max(target, v.level - rate);
          const float s = (float)(std::sin(v.phase) * v.level * v.velocity * gain_->get());
          v.phase += step;
          buffer.addSample(0, i, s);
          buffer.addSample(1, i, s);
        }
      }
      pos = to;
    };
    for (const auto m : midi) {
      render(m.samplePosition);
      const auto msg = m.getMessage();
      if (msg.isNoteOn() && msg.getNoteNumber() == kStateChangeNote) {
        // What a plugin does when the user loads a sample or a wavetable in its window: no parameter moves, it
        // only tells the host that its state is another one (VST3: IComponentHandler2::setDirty).
        updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
      } else if (msg.isNoteOn()) {
        Voice* free = &voices_[0];
        for (auto& v : voices_) if (!v.on && v.level <= 0) { free = &v; break; }
        *free = Voice{msg.getNoteNumber(), msg.getFloatVelocity(), 0, 0, true};
      } else if (msg.isNoteOff()) {
        for (auto& v : voices_) if (v.on && v.pitch == msg.getNoteNumber()) v.on = false;
      } else if (msg.isAllNotesOff()) {
        for (auto& v : voices_) v.on = false;
      }
    }
    render(buffer.getNumSamples());
  }
private:
  static constexpr int kStateChangeNote = 0;  // this note plays nothing: the plugin reports a change that is not a parameter
  struct Voice { int pitch = 0; float velocity = 0; double phase = 0, level = 0; bool on = false; };
  Voice voices_[16];
  double rate_ = 48000;
};
juce::AudioProcessor* make() { return new Synth(); }
#endif

#if CORE_TEST_GAIN
// Multiplies by gain (0.5 when new).
class Gain final : public TestPlugin {
public:
  Gain() : TestPlugin(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
  }
  void prepareToPlay(double, int) override {}
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.applyGain(gain_->get()); }
};
juce::AudioProcessor* make() { return new Gain(); }
#endif

#if CORE_TEST_DELAY
// Delays its input by kLatency samples and says so (setLatencySamples), as a lookahead limiter would.
class Delay final : public TestPlugin {
public:
  static constexpr int kLatency = 480;
  Delay() : TestPlugin(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    setLatencySamples(kLatency);
  }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
  }
  void prepareToPlay(double, int) override { for (auto& c : line_) c.fill(0.0f); at_ = 0; }
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      for (int c = 0; c < 2; ++c) std::swap(line_[(size_t)c][(size_t)at_], *buffer.getWritePointer(c, i));
      at_ = (at_ + 1) % kLatency;
    }
  }
private:
  std::array<std::array<float, kLatency>, 2> line_{};
  int at_ = 0;
};
juce::AudioProcessor* make() { return new Delay(); }
#endif

#if CORE_TEST_THROW
// Passes its input through for 0.1 s, then throws in every processBlock (audit 6.4, M3b): the engine must
// silence its track and play the others on.
class Throw final : public TestPlugin {
public:
  Throw() : TestPlugin(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
  }
  void prepareToPlay(double sampleRate, int) override { limit_ = (int64_t)(0.1 * sampleRate); played_ = 0; }
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
    if (played_ >= limit_) throw std::runtime_error("Core Test Throw: processBlock failed on purpose");
    played_ += buffer.getNumSamples();
  }
private:
  int64_t limit_ = 4800, played_ = 0;
};
juce::AudioProcessor* make() { return new Throw(); }
#endif

#if CORE_TEST_CRASH
// An instrument that is silent for 0.1 s and then crashes in processBlock (an access violation: a write through a
// null pointer). The engine must silence its track, play the others on, and never wait for this instance again: a
// crash caught by the host runs no destructor, so every lock held around the call stays held.
class Crash final : public TestPlugin {
public:
  Crash() : TestPlugin(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
  void prepareToPlay(double sampleRate, int) override { limit_ = (int64_t)(0.1 * sampleRate); played_ = 0; }
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
    buffer.clear();
    if (played_ >= limit_) *nowhere_ = 1;  // the crash, on purpose
    played_ += buffer.getNumSamples();
  }
private:
  int64_t limit_ = 4800, played_ = 0;
  volatile int* volatile nowhere_ = nullptr;  // volatile: the compiler cannot prove the write is invalid and drop it
};
juce::AudioProcessor* make() { return new Crash(); }
#endif

#if CORE_TEST_FAULT
// A gain effect that crashes on demand in one entry point, chosen by an environment variable (the engine process
// inherits it): CORE_TEST_FAULT_GETSTATE (getStateInformation, once it has played, so the crash lands when the host
// captures state and not while JUCE reads the default state at load), CORE_TEST_FAULT_SETSTATE (setStateInformation,
// at load), CORE_TEST_FAULT_RESET (reset), CORE_TEST_FAULT_EDITOR (createEditor), CORE_TEST_FAULT_PARAM (a parameter's
// getValue, as the host reads it for plugin.params). With none set it is an ordinary gain. The host must catch the
// crash, silence this plugin and play the others on, never taking the engine down (quality bar 2).
class FaultParam final : public juce::AudioParameterFloat {
public:
  FaultParam() : juce::AudioParameterFloat(juce::ParameterID{"fault", 1}, "Fault", 0.0f, 1.0f, 0.5f) {}
  float getValue() const override {
    if (envSet("CORE_TEST_FAULT_PARAM")) crashNow();
    return get();  // the range is 0..1, so the plain value is already normalized
  }
};

class Fault final : public TestPlugin {
public:
  Fault() : TestPlugin(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    addParameter(new FaultParam());
  }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
  }
  void prepareToPlay(double, int) override {}
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.applyGain(gain_->get()); played_ += buffer.getNumSamples(); }
  void getStateInformation(juce::MemoryBlock& data) override {
    if (played_ > 0 && envSet("CORE_TEST_FAULT_GETSTATE")) crashNow();
    TestPlugin::getStateInformation(data);
  }
  void setStateInformation(const void* data, int size) override {
    if (envSet("CORE_TEST_FAULT_SETSTATE")) crashNow();
    TestPlugin::setStateInformation(data, size);
  }
  void reset() override { if (envSet("CORE_TEST_FAULT_RESET")) crashNow(); }
  juce::AudioProcessorEditor* createEditor() override {
    if (envSet("CORE_TEST_FAULT_EDITOR")) crashNow();
    return TestPlugin::createEditor();
  }
private:
  int64_t played_ = 0;
};
juce::AudioProcessor* make() { return new Fault(); }
#endif

}  // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return make(); }
