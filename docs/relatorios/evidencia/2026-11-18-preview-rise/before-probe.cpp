#include "edit.hpp"
#include "pattern_sound.hpp"
#include "core/assets.hpp"
#include <iostream>
int main() {
  auto doc = core::doc::newTemplateDocument();
  auto& track = doc.tracks.front();
  track.instrument.kind = "builtin";
  track.instrument.name = "wave";
  auto request = core::preview::makeRequest(doc, track.id, track.patterns.front().id);
  if (!request) return 1;
  core::AssetStore assets;
  auto sound = core::preview::render(*request, assets);
  std::cout << "request created; waveform empty=" << sound.empty() << "; notes=" << track.patterns.front().notes.size() << '\n';
  return sound.empty() ? 0 : 2;
}
