#include "device.hpp"
#include "../dsp/float_mode.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "miniaudio.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <avrt.h>
#ifdef _MSC_VER
#pragma comment(lib, "avrt.lib")
#endif
#endif

namespace core {
namespace {

// M5: the live workers (RtWorkerPool) get what the device's own thread gets: the MMCSS "Pro Audio" task
// and time-critical priority, so a busy PC does not delay them.
void proAudioThread() {
#ifdef _WIN32
  DWORD task = 0;
  AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
}

ma_backend backendFromName(const std::string& name) {
  if (name == "wasapi") return ma_backend_wasapi;
  if (name == "dsound") return ma_backend_dsound;
  if (name == "winmm") return ma_backend_winmm;
  if (name == "coreaudio") return ma_backend_coreaudio;
  if (name == "pulseaudio") return ma_backend_pulseaudio;
  if (name == "alsa") return ma_backend_alsa;
  if (name == "jack") return ma_backend_jack;
  if (name == "null") return ma_backend_null;
  throw std::runtime_error("Unknown audio backend '" + name + "'.");
}

class MiniaudioOutput final : public AudioOutput {
  ma_context context_{};
  ma_device device_{};
  bool contextReady_ = false, deviceReady_ = false;
  Engine* engine_ = nullptr;
  std::vector<float> L_, R_;
  uint64_t lastStartNs_ = 0;
  double sampleRate_ = 48000;
  // ART-003: the device's last notification, written by the notification callback (device thread) and read
  // by the control thread (Server::engine.stats / stateEvent). 0 running, 1 stopped, 2 rerouted. An atomic
  // store/load only: no lock, allocation or I/O on the device thread.
  std::atomic<int> deviceNotification_{0};

  // miniaudio: the device stopped (unplugged), was rerouted (the default device changed), or started.
  // "Do not restart or uninitialize the device from the callback" (miniaudio.h) — only flag it here; the
  // app learns it from the Server and reopens. The stopped notification does not fire on a reroute.
  static void notification(const ma_device_notification* n) {
    auto* self = static_cast<MiniaudioOutput*>(n->pDevice->pUserData);
    switch (n->type) {
      case ma_device_notification_type_stopped: self->deviceNotification_.store(1, std::memory_order_release); break;
      case ma_device_notification_type_rerouted: self->deviceNotification_.store(2, std::memory_order_release); break;
      case ma_device_notification_type_started: self->deviceNotification_.store(0, std::memory_order_release); break;
      default: break;  // interruption_began/ended, unlocked: mobile backends, not used on Windows
    }
  }

  static void callback(ma_device* device, void* output, const void*, ma_uint32 frames) {
    auto* self = static_cast<MiniaudioOutput*>(device->pUserData);
    setAudioFloatMode();  // FTZ + DAZ, whatever the backend did (float_mode.hpp); the workers take it from here
    const uint64_t start = nowNs();
    float* out = static_cast<float*>(output);
    ma_uint32 done = 0;
    // An exception must not leave the device's thread (it would end the engine): a plugin that throws
    // costs that buffer, which is silence, and is counted (engine.stats callbackErrors).
    try {
      while (done < frames) {
        ma_uint32 n = std::min<ma_uint32>(frames - done, (ma_uint32)self->L_.size());
        self->engine_->process(self->L_.data(), self->R_.data(), (int)n);
        for (ma_uint32 i = 0; i < n; ++i) {
          out[2 * (done + i)] = self->L_[i];
          out[2 * (done + i) + 1] = self->R_[i];
        }
        done += n;
      }
    } catch (...) {
      std::memset(out, 0, sizeof(float) * 2 * frames);
      self->engine_->stats().callbackErrors.fetch_add(1, std::memory_order_relaxed);
    }
    const uint64_t end = nowNs();
    const uint64_t budget = (uint64_t)(frames * 1e9 / self->sampleRate_);
    const uint64_t gap = self->lastStartNs_ ? start - self->lastStartNs_ : 0;
    self->lastStartNs_ = start;
    self->engine_->recordCallback(end - start, budget, gap, frames);
  }

  void initContext(const json& args) {
    if (contextReady_) return;
    ma_result r;
    // The audio thread at the top priority the OS gives a normal process (Windows: time critical), so a
    // build or another busy program on the same PC does not starve it (2026-11-04: crackles while a full
    // MSVC build used every core).
    ma_context_config cc = ma_context_config_init();
    cc.threadPriority = ma_thread_priority_realtime;
    if (args.contains("backend") && args["backend"].is_string()) {
      ma_backend b = backendFromName(args["backend"].get<std::string>());
      r = ma_context_init(&b, 1, &cc, &context_);
    } else {
      r = ma_context_init(nullptr, 0, &cc, &context_);
    }
    if (r != MA_SUCCESS) throw std::runtime_error(std::string("Audio backend failed: ") + ma_result_description(r));
    contextReady_ = true;
  }

public:
  ~MiniaudioOutput() override { stop(); if (contextReady_) ma_context_uninit(&context_); }

  // ART-003: 0 running, 1 stopped, 2 rerouted — read by the Server (control thread), written by the
  // notification callback (device thread). An atomic load only.
  int deviceNotification() const override { return deviceNotification_.load(std::memory_order_acquire); }

  json devices(const json& args) override {
    initContext(args);
    ma_device_info* playback; ma_uint32 count;
    json list = json::array();
    if (ma_context_get_devices(&context_, &playback, &count, nullptr, nullptr) == MA_SUCCESS)
      for (ma_uint32 i = 0; i < count; ++i) list.push_back({{"name", playback[i].name}, {"default", (bool)playback[i].isDefault}});
    return {{"backend", ma_get_backend_name(context_.backend)}, {"devices", list}};
  }

  json start(Engine& engine, const json& args) override {
    stop();
    deviceNotification_.store(0, std::memory_order_release);  // a fresh start: any previous loss is cleared
    initContext(args);
    engine_ = &engine;
    sampleRate_ = engine.sampleRate();
    const int block = args.value("blockSize", 128);
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    // Default: the device's own rate, so Windows never resamples (the engine is rebuilt for it).
    config.sampleRate = args.contains("sampleRate") ? (ma_uint32)args["sampleRate"].get<double>() : 0;
    config.periodSizeInFrames = (ma_uint32)block;
    config.periods = (ma_uint32)args.value("periods", 2);
    config.performanceProfile = ma_performance_profile_low_latency;
    config.noPreSilencedOutputBuffer = MA_TRUE;  // the engine clears its buffers
    config.noClip = MA_TRUE;
    config.dataCallback = callback;
    config.notificationCallback = notification;  // ART-003: stopped / rerouted reach the engine and the app
    config.pUserData = this;
    config.wasapi.noAutoConvertSRC = MA_TRUE;
    config.wasapi.noDefaultQualitySRC = MA_TRUE;
    // Windows' multimedia scheduler (MMCSS) "Pro Audio" task, as DAWs do: the audio thread is scheduled
    // ahead of ordinary work even when every core is busy.
    config.wasapi.usage = ma_wasapi_usage_pro_audio;
    rtThreadInit().store(&proAudioThread);
    if (args.value("exclusive", false)) config.playback.shareMode = ma_share_mode_exclusive;
    // Find the requested device by name (substring).
    ma_device_id id; bool haveId = false;
    if (args.contains("device") && args["device"].is_string()) {
      ma_device_info* playback; ma_uint32 count;
      if (ma_context_get_devices(&context_, &playback, &count, nullptr, nullptr) == MA_SUCCESS)
        for (ma_uint32 i = 0; i < count; ++i)
          if (std::strstr(playback[i].name, args["device"].get<std::string>().c_str())) { id = playback[i].id; haveId = true; break; }
      if (!haveId) throw std::runtime_error("No output device matching '" + args["device"].get<std::string>() + "'.");
      config.playback.pDeviceID = &id;
    }
    L_.assign(4096, 0.0f); R_.assign(4096, 0.0f);
    lastStartNs_ = 0;
    ma_result r = ma_device_init(&context_, &config, &device_);
    if (r != MA_SUCCESS) throw std::runtime_error(std::string("Cannot open the audio device: ") + ma_result_description(r));
    deviceReady_ = true;
    sampleRate_ = device_.sampleRate;
    if (device_.sampleRate != (ma_uint32)engine.sampleRate()) {
      // Caller rebuilds the engine at this rate and calls start again.
      json info = {{"sampleRate", (double)device_.sampleRate}};
      stop();
      return info;
    }
    engine.setDeviceBuffer((uint64_t)((double)device_.playback.internalPeriodSizeInFrames * device_.playback.internalPeriods * 1e9 / device_.sampleRate));
    r = ma_device_start(&device_);
    if (r != MA_SUCCESS) { stop(); throw std::runtime_error(std::string("Cannot start the audio device: ") + ma_result_description(r)); }
    deviceNotification_.store(0, std::memory_order_release);  // running again (the started notification also clears it)
    const double period = device_.playback.internalPeriodSizeInFrames;
    const double periods = device_.playback.internalPeriods;
    return {{"sampleRate", (double)device_.sampleRate}, {"blockSize", (int)period},
            {"periods", (int)periods}, {"latencyMs", period * periods * 1000.0 / device_.sampleRate},
            {"internalSampleRate", device_.playback.internalSampleRate},
            {"backend", ma_get_backend_name(context_.backend)}, {"device", device_.playback.name},
            {"exclusive", device_.playback.shareMode == ma_share_mode_exclusive}, {"scheduling", "Pro Audio"}};
  }

  void stop() override {
    if (deviceReady_) { ma_device_uninit(&device_); deviceReady_ = false; }
  }
};

} // namespace

std::unique_ptr<AudioOutput> makeMiniaudioOutput() { return std::make_unique<MiniaudioOutput>(); }

} // namespace core
