// The user-sample host (BUG-SWEEP-10 positive; owner ratified 2026-11-18: «sampler = host de samples do
// utilizador, sem fontes de som próprias.»): it PLAYS the user's audio files on notes — no oscillator, no
// filter, no sound of its own. The graph loads the zones' audio through the AssetStore (the audio clips'
// path) on the control thread; the audio thread only reads the finished buffers, never allocates.
//
// Playback contract (the host's parameters, in the registry's order, see instruments.cpp):
//   level     dB gain over the file (-24..24)
//   start     where playback begins in the file (ms)
//   pitched   1: the key transposes the file around the zone's rootKey; 0: every key plays it as it is
//   release   a looped zone's fade-out after note-off (ms)
//   retrigger 1: a note-on restarts the zone (its previous voice fades fast); 0: a sounding zone is not
//             restarted (a roll of notes plays a riser once)
//   decay     a pluck that falls to -60 dB over this time while the note sounds (ms; 10000 = off)
// A one-shot (no loop) plays to the file's end: the file's own tail is the sound; the pattern's gate
// retriggers it, never cuts it. A looped zone loops while the note is held and releases on note-off.
// Every envelope is a function of the voice's absolute age/position, so the render is the same whatever
// the block size (real time == export, bit for bit).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include "instruments.hpp"
#include "../io/audio_file.hpp"

namespace core {

// The host's sound version (matches the document's kSamplerVersion): the sound is "play the user's file",
// stable across versions; a song keeps the version it was saved with.
constexpr int kSampleHostVersion = 4;

// One key range of the host, with its audio (null data: the file could not be loaded — the zone is silent,
// reported by the graph as a missing asset). loopStart/loopEnd are frames at the graph's rate; loopEnd 0
// means a one-shot.
struct SampleHostZone {
  std::shared_ptr<const AudioData> data;
  int rootKey = 60, loKey = 0, hiKey = 127;
  int64_t loopStart = 0, loopEnd = 0;
};

class SampleHost final : public Instrument {
public:
  SampleHost(double sampleRate, std::vector<SampleHostZone> zones) : Instrument(sampleRate), zones_(std::move(zones)) {}
  const char* name() const override { return "sample"; }
  bool stereo() const override { return true; }
  void noteOn(int pitch, double velocity) override { queueNote(0, true, pitch, velocity); }
  void noteOff(int pitch) override { queueNote(0, false, pitch, 0); }
  void queueNote(int offset, bool on, int pitch, double velocity) override {
    pitch &= 127;
    if (count_ < kMaxNotes) { insert(NoteEvent{offset, on, pitch, (float)velocity}, count_); return; }
    if (on) { ++droppedNotes_; return; }  // a note-on cannot be played; the off always fits (reserve below)
    if (pendingOffs_[pitch]++ == 0) pendingAt_[pitch] = offset;
  }
  // Stop, seek, loop wrap, panic: the host owns its voices, so they go at once (chase() picks up the ones
  // that still sound at the new position). No queue event needed.
  void allNotesOff() override {
    for (auto& v : voices_) v.zone = -1;
    count_ = 0;
    for (int p = 0; p < 128; ++p) { pendingOffs_[p] = 0; pendingAt_[p] = 0; }
  }
  void render(float*, int) override {}
  void renderStereo(float* left, float* right, int frames) override {
    int count = count_;
    // The note-offs that did not fit in the queue go in at their own sample (never lost: a looped voice
    // would hang). Any that do not fit in this block's reserve go in at the start of the next one.
    for (int pass = 0; pass < 2; ++pass)
      for (int p = 0; p < 128; ++p)
        while (pendingOffs_[p] > 0 && count < kCapacity) {
          insert(NoteEvent{std::min(pendingAt_[p], frames - 1), false, p, 0.0f}, count);
          --pendingOffs_[p];
          if (pass == 0) break;
        }
    for (int p = 0; p < 128; ++p) pendingAt_[p] = 0;
    // Render the spans between events, so a note starts at its exact sample (the same at any block size).
    int cursor = 0;
    for (int i = 0; i < count; ++i) {
      const NoteEvent& e = notes_[i];
      const int at = std::clamp(e.offset, 0, frames);
      if (at > cursor) { span(left + cursor, right + cursor, at - cursor); cursor = at; }
      if (e.on) startVoice(e.pitch, e.velocity, 0.0);
      else releaseVoice(e.pitch);
    }
    if (cursor < frames) span(left + cursor, right + cursor, frames - cursor);
    count_ = 0;
  }
  // A note caught already sounding (a seek or a loop jump into it): pick it up where it would be, `age`
  // samples in, with a 3 ms fade-in (the base contract).
  void chaseNote(int pitch, double velocity, double age) override { startVoice(pitch, velocity, age); }
  void adopt(const Instrument& previous) override {
    const auto* p = dynamic_cast<const SampleHost*>(&previous);
    if (!p || p->zones_.size() != zones_.size()) return;
    for (size_t i = 0; i < zones_.size(); ++i)
      if (zones_[i].data.get() != p->zones_[i].data.get()) return;  // another file: the new sound starts fresh
    for (int i = 0; i < kMaxVoices; ++i) voices_[i] = p->voices_[i];
    for (int i = 0; i < p->count_; ++i) notes_[i] = p->notes_[i];
    count_ = p->count_;
    for (int i = 0; i < 128; ++i) { pendingOffs_[i] = p->pendingOffs_[i]; pendingAt_[i] = p->pendingAt_[i]; }
  }

private:
  struct NoteEvent { int offset; bool on; int pitch; float velocity; };
  struct Voice {
    int zone = -1;          // index into zones_; -1 = free
    int pitch = 0;
    double pos = 0;         // read position in the file (frames, fractional)
    double rate = 1;        // 2^((pitch-rootKey)/12) when pitched
    double age = 0;         // frames since the note-on (attack, pluck)
    double attack = 1;      // fade-in length (frames)
    float gain = 0;         // velocity (the level is applied at render, so it can glide)
    bool releasing = false;
    double releaseLeft = 0, releaseTotal = 1;
  };
  static constexpr int kMaxVoices = 32;
  static constexpr int kCapacity = 1024;             // events one block may hold
  static constexpr int kMaxNotes = kCapacity - 128;  // the rest: reserved note-offs

  // After the events already at that sample (stable: an off and an on at one sample keep their order).
  void insert(const NoteEvent& e, int& count) {
    int at = count;
    while (at > 0 && notes_[at - 1].offset > e.offset) { notes_[at] = notes_[at - 1]; --at; }
    notes_[at] = e;
    ++count;
  }
  void startVoice(int pitch, double velocity, double ageSamples) {
    int zi = -1;
    for (size_t i = 0; i < zones_.size(); ++i)
      if (zones_[i].data && pitch >= zones_[i].loKey && pitch <= zones_[i].hiKey) { zi = (int)i; break; }
    if (zi < 0) return;  // no zone plays this key (or its file is missing): silent, already reported
    const SampleHostZone& z = zones_[(size_t)zi];
    const bool retrigger = params[4] >= 0.5;
    if (!retrigger) {
      for (const auto& v : voices_)
        if (v.zone == zi && !v.releasing) return;  // a roll plays the riser once
    } else {
      // Restart: the zone's previous voice fades fast (5 ms, no click) instead of stacking on itself.
      const double choke = std::max(1.0, 0.005 * sampleRate_);
      for (auto& v : voices_)
        if (v.zone == zi && !v.releasing) { v.releasing = true; v.releaseTotal = choke; v.releaseLeft = choke; }
    }
    int slot = -1;
    double oldest = -1;
    for (int i = 0; i < kMaxVoices; ++i) {
      if (voices_[i].zone < 0) { slot = i; break; }
      if (voices_[i].age > oldest) { oldest = voices_[i].age; slot = i; }
    }
    Voice& v = voices_[slot];
    const double frames = (double)z.data->frames();
    const double rate = params[2] >= 0.5 ? std::pow(2.0, (pitch - z.rootKey) / 12.0) : 1.0;
    const double start = params[1] * sampleRate_ / 1000.0;
    v.zone = zi;
    v.pitch = pitch;
    v.rate = rate;
    v.pos = std::clamp(start + ageSamples * rate, 0.0, frames - 1);
    v.age = ageSamples;
    v.attack = std::max(1.0, (ageSamples > 0 ? 0.003 : 0.0001) * sampleRate_);  // 0.1 ms, or 3 ms when chased
    v.gain = (float)std::clamp(velocity, 0.0, 1.0);
    v.releasing = false;
    v.releaseLeft = 0;
    v.releaseTotal = 1;
    if (z.loopEnd == 0 && v.pos >= frames - 1) v.zone = -1;  // a one-shot chased past its end: nothing to play
  }
  void releaseVoice(int pitch) {
    const double rel = std::max(1.0, params[3] * sampleRate_ / 1000.0);
    for (auto& v : voices_)
      if (v.zone >= 0 && v.pitch == pitch && !v.releasing && zones_[(size_t)v.zone].loopEnd > 0) {
        v.releasing = true;  // a looped zone fades out; a one-shot plays its own tail to the end
        v.releaseTotal = rel;
        v.releaseLeft = rel;
      }
  }
  void span(float* left, float* right, int n) {
    const double level = std::pow(10.0, params[0] / 20.0);
    const double decayFrames = params[5] * sampleRate_ / 1000.0;
    const bool pluck = params[5] < 9999.0;  // the default 10000 ms is "no pluck"
    for (int i = 0; i < n; ++i) {
      double l = 0, r = 0;
      for (auto& v : voices_) {
        if (v.zone < 0) continue;
        const SampleHostZone& z = zones_[(size_t)v.zone];
        const AudioData& d = *z.data;
        const double frames = (double)d.frames();
        const double pos = std::clamp(v.pos, 0.0, frames - 1);
        const int64_t i0 = (int64_t)pos;
        const int64_t i1 = std::min<int64_t>(i0 + 1, d.frames() - 1);
        const double f = pos - (double)i0;
        const double sl = d.L[(size_t)i0] + (d.L[(size_t)i1] - d.L[(size_t)i0]) * f;
        const double sr = d.R[(size_t)i0] + (d.R[(size_t)i1] - d.R[(size_t)i0]) * f;
        double g = v.gain * level;
        if (v.age < v.attack) g *= v.age / v.attack;
        if (pluck) g *= std::pow(10.0, -3.0 * std::min(1.0, v.age / decayFrames));  // -60 dB at decay
        if (v.releasing) g *= std::max(0.0, v.releaseLeft / v.releaseTotal);
        l += sl * g;
        r += sr * g;
        v.pos += v.rate;
        v.age += 1;
        if (v.releasing && --v.releaseLeft <= 0) { v.zone = -1; continue; }
        if (z.loopEnd > 0) {
          if (v.pos >= (double)z.loopEnd) v.pos = (double)z.loopStart + (v.pos - (double)z.loopEnd);
        } else if (v.pos >= frames - 1) { v.zone = -1; continue; }
        if (pluck && v.age >= decayFrames) { v.zone = -1; continue; }
      }
      left[i] += (float)l;
      right[i] += (float)r;
    }
  }
  std::vector<SampleHostZone> zones_;  // built on the control thread, never changed while playing
  Voice voices_[kMaxVoices];
  NoteEvent notes_[kCapacity];
  int count_ = 0;
  int pendingOffs_[128] = {};
  int pendingAt_[128] = {};
};

}  // namespace core