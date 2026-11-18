#include "Waveforms.h"
#include <cmath>

namespace core::app {

WaveformCache::WaveformCache(std::function<void()> onReady) : onReady_(std::move(onReady)) { formats_.registerBasicFormats(); }

WaveformCache::~WaveformCache() {
  *alive_ = false;
  pool_.removeAllJobs(true, 10000);
}

std::shared_ptr<const Waveform> WaveformCache::get(const std::string& id, const juce::File& file) {
  // The file's size and date tell a replaced file apart; asked of the disk at most every 2 s per file, not on
  // every repaint of every clip.
  std::string assetId;
  {
    const std::string key = id + "|" + file.getFullPathName().toStdString();
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    std::lock_guard<std::mutex> g(mutex_);
    auto& seen = stats_[key];
    if (seen.version.empty() || now - seen.checkedAt > 2000) {
      seen.version = std::to_string(file.getSize()) + "|" + std::to_string(file.getLastModificationTime().toMilliseconds());
      seen.checkedAt = now;
    }
    assetId = id + "|" + seen.version;
  }
  {
    std::lock_guard<std::mutex> g(mutex_);
    if (auto it = ready_.find(assetId); it != ready_.end()) return it->second;
    if (started_[assetId]) return nullptr;
    started_[assetId] = true;
  }
  std::weak_ptr<std::atomic<bool>> alive = alive_;
  pool_.addJob([this, assetId, file, alive] {
    std::unique_ptr<juce::AudioFormatReader> reader(formats_.createReaderFor(file));
    if (!reader) return;  // unreadable: no waveform (the clip still shows)
    auto wave = std::make_shared<Waveform>();
    wave->sampleRate = reader->sampleRate;
    const int chunk = wave->framesPerPeak * 64;
    juce::AudioBuffer<float> buf((int)reader->numChannels, chunk);
    for (juce::int64 at = 0; at < reader->lengthInSamples; at += chunk) {
      if (auto a = alive.lock(); !a || !*a) return;
      const int n = (int)std::min<juce::int64>(chunk, reader->lengthInSamples - at);
      reader->read(&buf, 0, n, at, true, true);
      for (int s = 0; s < n; s += wave->framesPerPeak) {
        float peak = 0, rms = 0;
        const int len = std::min(wave->framesPerPeak, n - s);
        for (int c = 0; c < buf.getNumChannels(); ++c) {
          peak = std::max(peak, buf.getMagnitude(c, s, len));
          rms = std::max(rms, buf.getRMSLevel(c, s, len));
        }
        wave->peaks.push_back(peak);
        wave->rms.push_back(rms);
      }
    }
    {
      if (auto a = alive.lock(); !a || !*a) return;
      std::lock_guard<std::mutex> g(mutex_);
      ready_[assetId] = wave;
    }
    juce::MessageManager::callAsync([this, alive] { if (auto a = alive.lock(); a && *a && onReady_) onReady_(); });
  });
  return nullptr;
}

void drawWaveColumn(juce::Graphics& g, const Waveform& w, float x, float mid, float half, double from, double to, juce::Colour colour,
                    float gain) {
  const float peak = std::min(1.0f, w.peakBetween(from, to) * gain);
  const float body = std::min(peak, rmsBetween(w, from, to) * 1.41f * gain);
  const float hp = peak * half, hb = body * half;
  if (hp < 0.5f) {
    g.setColour(colour.withMultipliedAlpha(0.35f));
    g.fillRect(x, mid - 0.5f, 1.0f, 1.0f);  // silence: only the centre line
    return;
  }
  g.setColour(colour.withMultipliedAlpha(0.42f));
  g.fillRect(x, mid - hp, 1.0f, 2 * hp);
  if (hb >= 0.5f) {
    g.setColour(colour.brighter(0.2f));
    g.fillRect(x, mid - hb, 1.0f, 2 * hb);
  }
}

void drawWaveform(juce::Graphics& g, const Waveform& w, juce::Rectangle<int> area, double from, double to, juce::Colour colour, float pad,
                  float gain) {
  const int width = std::max(1, area.getWidth());
  const float mid = (float)area.getCentreY(), half = area.getHeight() / 2.0f - pad;
  const auto clip = g.getClipBounds();
  const int x0 = std::max(area.getX(), clip.getX()), x1 = std::min(area.getRight(), clip.getRight());
  for (int x = x0; x < x1; ++x) {
    const double a = from + (to - from) * (x - area.getX()) / width, b = from + (to - from) * (x + 1 - area.getX()) / width;
    drawWaveColumn(g, w, (float)x, mid, half, a, b, colour, gain);
  }
}

} // namespace core::app
