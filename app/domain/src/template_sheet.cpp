#include "template_sheet.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include "edit.hpp"

namespace core::doc {

namespace {

constexpr int64_t kBarTicks = 4 * kPpq;

std::string num(double v) {
  if (!std::isfinite(v)) return "?";
  char buf[48];
  // 2 decimals at most, no exponent: 20000 Hz, 0.707, 251.9 ms.
  if (std::fabs(v) >= 1000) std::snprintf(buf, sizeof buf, "%.0f", v);
  else std::snprintf(buf, sizeof buf, "%.4g", v);
  return buf;
}

std::string bars(int64_t ticks) {  // 1-based bar and beat, as on the ruler: 9.1 = bar 9, beat 1
  const int64_t bar = ticks / kBarTicks + 1, beat = (ticks % kBarTicks) / kPpq + 1;
  return std::to_string(bar) + "." + std::to_string(beat);
}

std::string length(int64_t ticks) { return num((double)ticks / kBarTicks) + " compassos"; }

const char* kNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
std::string noteName(int pitch) { return std::string(kNoteNames[((pitch % 12) + 12) % 12]) + std::to_string(pitch / 12 - 1); }

// The track's instrument: a plugin (its name, vendor and id), or no instrument yet. Core makes no sound of its
// own (Diogo 2026-11-18): the built-in instruments, their parameters, the sampler and the W-Core modulation are gone.
void instrumentPart(std::ostringstream& o, const Track& t) {
  const Instrument& ins = t.instrument;
  if (ins.kind == "plugin" && ins.plugin.is_object()) {
    o << "- Instrumento: " << ins.plugin.value("name", std::string("Plugin"));
    const std::string vendor = ins.plugin.value("vendor", std::string());
    if (!vendor.empty()) o << " (" << vendor << ")";
    o << " (`plugin:" << ins.plugin.value("id", std::string()) << "`)\n";
    return;
  }
  o << "- Instrumento: " << (ins.name.empty() ? ins.kind : ins.name) << " (`" << ins.kind << "`)\n";
}

// The chain as it is: each effect's name, id and bypass, and its stored parameters as they are (a plugin's state
// belongs to the plugin; there is no built-in parameter catalogue any more).
void effectsPart(std::ostringstream& o, const json& effects) {
  if (!effects.is_array() || effects.empty()) return;
  o << "\nEfeitos (pela ordem da cadeia):\n";
  for (const auto& fx : effects) {
    if (!fx.is_object()) continue;
    const bool plugin = fx.value("type", std::string()) == "plugin" && fx.contains("plugin");
    const std::string name = plugin ? fx["plugin"].value("name", std::string("Plugin")) : effectName(fx);
    o << "\n- **" << (name.empty() ? std::string("Efeito") : name) << "** (id `" << fx.value("id", std::string()) << "`)"
      << (effectBypassed(fx) ? " DESLIGADO" : "") << "\n";
    if (fx.contains("params") && fx["params"].is_object() && !fx["params"].empty()) {
      o << "\n  | Parâmetro | Valor |\n  |---|---|\n";
      for (const auto& [k, v] : fx["params"].items()) o << "  | `" << k << "` | " << v.dump() << " |\n";
    }
  }
}

void automationPart(std::ostringstream& o, const std::vector<Automation>& list) {
  if (list.empty()) return;
  o << "\nAutomação:\n\n| Parâmetro | Pontos | Mín. | Máx. | Começa | Acaba |\n|---|---|---|---|---|---|\n";
  for (const auto& a : list) {
    if (a.points.empty()) continue;
    double lo = a.points.front().value, hi = lo;
    for (const auto& p : a.points) { lo = std::min(lo, p.value); hi = std::max(hi, p.value); }
    o << "| `" << a.param << "` | " << a.points.size() << " | " << num(lo) << " | " << num(hi) << " | " << num(a.points.front().value)
      << " em " << bars(a.points.front().tick) << " | " << num(a.points.back().value) << " em " << bars(a.points.back().tick) << " |\n";
  }
}

void notesPart(std::ostringstream& o, const Track& t) {
  int64_t count = 0;
  int lo = 127, hi = 0;
  for (const auto& c : t.clips) {
    const Pattern* p = t.findPattern(c.patternId);
    if (!p || p->lengthTicks <= 0) continue;
    for (int64_t at = -c.offsetTicks; at < c.lengthTicks; at += p->lengthTicks)
      for (const auto& n : p->notes)
        if (at + n.tick >= 0 && at + n.tick < c.lengthTicks) { ++count; lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
  }
  if (count) o << "- Notas: " << count << ", de " << noteName(lo) << " a " << noteName(hi) << ", em " << t.clips.size() << " clips e "
               << t.patterns.size() << " padrões\n";
  if (!t.audioClips.empty()) o << "- Clips de áudio: " << t.audioClips.size() << " (o template não leva áudio)\n";
}

} // namespace

std::string templateCompanionPath(const std::string& templateFile, const std::string& ending) {
  const std::string ext = kTemplateExtension;
  std::string base = templateFile;
  if (base.size() >= ext.size() && base.compare(base.size() - ext.size(), ext.size(), ext) == 0) base.resize(base.size() - ext.size());
  return base + ending;
}

std::string templateSheet(const SongTemplate& t) {
  const Document& d = t.document;
  std::ostringstream o;
  o << "# " << t.info.name << "\n\n";
  o << "Ficha do template `" << t.info.id << "` (versão " << t.info.version << "). Estilo: " << (t.info.style.empty() ? "—" : t.info.style) << ".\n";
  if (!t.info.description.empty()) o << "\n" << t.info.description << "\n";
  if (t.info.source.contains("from"))
    o << "\nFeito a partir de \"" << t.info.source.value("from", std::string()) << "\", do tick " << t.info.source.value("startTick", (int64_t)0)
      << " ao " << t.info.source.value("endTick", (int64_t)0) << " (compassos " << bars(t.info.source.value("startTick", (int64_t)0)) << " a "
      << bars(t.info.source.value("endTick", (int64_t)0)) << ").\n";

  o << "\n## Tempo e forma\n\n- BPM: " << num(d.bpm) << "\n- Duração: " << length(d.lengthTicks) << " (" << d.lengthTicks << " ticks, "
    << kPpq << " por semínima)\n";
  for (const auto& p : d.tempoMap) o << "- Tempo " << num(p.bpm) << " BPM em " << bars(p.tick) << (p.ramp ? " (em rampa)" : "") << "\n";
  if (!d.sections.empty()) {
    o << "\n| Secção | Começa | Duração |\n|---|---|---|\n";
    for (const auto& s : d.sections) o << "| " << s.name << " | " << bars(s.startTick) << " | " << length(s.lengthTicks) << " |\n";
  }

  o << "\n## Master\n\n- Ganho: " << num(d.masterGainDb) << " dB\n";
  effectsPart(o, d.masterEffects);
  automationPart(o, d.masterAutomation);

  o << "\n## Pistas\n";
  for (const auto& tr : d.tracks) {
    o << "\n### " << tr.name << " (`" << tr.id << "`)\n\n";
    instrumentPart(o, tr);
    o << "\n- Mistura: ganho " << num(tr.mix.gainDb) << " dB, pan " << num(tr.mix.pan) << (tr.mix.mute ? ", mudo" : "")
      << (tr.mix.output.empty() ? "" : ", saída para `" + tr.mix.output + "`") << "\n";
    for (const auto& s : tr.mix.sends)
      o << "- Envio para `" << s.to << "`: " << num(s.gainDb) << " dB" << (s.pre ? " (antes do fader)" : "") << "\n";
    notesPart(o, tr);
    effectsPart(o, tr.effects);
    automationPart(o, tr.automation);
  }
  return o.str();
}

} // namespace core::doc
