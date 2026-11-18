#include "core_plugins.hpp"

#include <cctype>
#include <set>
#include "roles.hpp"

namespace core::doc {

namespace {
std::string lower(std::string s) {
  for (auto& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}
}  // namespace

const std::vector<CorePluginInfo>& corePlugins() {
  // The legacy module titles (P-Core, W-Core, F-Core, R-Core, D-Core) left the cards (Diogo 2026-11-18:
  // "apenas o pattern maker,mixer, piano, tools. o que esta a causar erros limpa, w-core, d-core f-core,
  // tudo isso ja nao entra"). The ids stay, and the old titles are aliases below: a legacy project or the
  // AI's old names still find the same tool.
  static const std::vector<CorePluginInfo> list = {
      {"pcore", "Pattern Maker", "Patterns", "writes rhythms and notes in layers: part, shape (KBB...), scale, grid, changes", {}, "window",
       {"patterns", "pattern core", "channel rack", "p-core"}},
      {"wcore", "Instrument", "By role", "the track plays the plugin of a role: bass, lead, pad, pluck or drums", {"bass", "lead", "pad", "pluck", "drums"},
       "tool", {"synth", "role instrument", "w-core", "w-core synth"}},
      {"fcore", "Effects", "By intent", "effects by intent over a section: build-up, suck, intro, outro, impact, break", {"filter", "reverb", "delay", "drive"},
       "tool", {"fx", "fx core", "intent", "f-core"}},
      {"rcore", "Reverb", "Shared bus", "the selected tracks share one reverb on a bus that follows the sections", {"reverb"}, "tool",
       {"reverb core", "shared reverb", "space", "r-core"}},
      {"dcore", "Delay", "Shared bus", "the selected tracks share one echo on a bus that follows the sections", {"delay"}, "tool",
       {"delay core", "shared delay", "echo", "d-core"}},
      {"suck", "SUCK", "Into the drop", "the sound is sucked into the drop: the reverb opens, the filter closes, back on the drop",
       {"reverb", "filter"}, "tool", {"suck into the drop"}},
      {"pum", "PUM", "Sidechain", "the volume dips at every kick and comes back", {"sidechain"}, "tool", {"pump", "sidechain pump"}},
      {"buildup", "Build-up", "Up to the drop", "filter opens, reverb and delay grow, drive pushes, back on the drop",
       {"filter", "reverb", "delay", "drive"}, "tool", {"build up", "buildup fx"}},
      {"roll", "ROLL", "Roll", "a roll that speeds up into the drop, in a new pattern on the track", {}, "tool", {"snare roll", "kick roll"}},
  };
  return list;
}

const CorePluginInfo* findCorePlugin(const std::string& idOrName) {
  const std::string want = lower(idOrName);
  if (want.empty()) return nullptr;
  for (const auto& p : corePlugins()) {
    if (p.id == want || lower(p.shortName) == want) return &p;
    for (const auto& a : p.aliases) if (a == want) return &p;
  }
  return nullptr;
}

std::vector<std::string> corePluginListProblems() {
  std::vector<std::string> out;
  std::set<std::string> names;
  for (const auto& p : corePlugins()) {
    if (p.id.empty() || p.shortName.empty() || p.word.empty() || p.about.empty()) out.push_back(p.id + ": an empty field");
    if (p.opens != "window" && p.opens != "tool") out.push_back(p.id + ": opens is window or tool");
    for (const auto& n : std::vector<std::string>{p.id, lower(p.shortName)})
      if (!names.insert(n).second && n != p.id) out.push_back(p.id + ": '" + n + "' is used twice");
    for (const auto& a : p.aliases) {
      if (a != lower(a)) out.push_back(p.id + ": alias '" + a + "' is not lower case");
      if (!names.insert(a).second) out.push_back(p.id + ": alias '" + a + "' is used twice");
    }
    for (const auto& r : p.roles) if (!findRole(r)) out.push_back(p.id + ": unknown role '" + r + "'");
  }
  return out;
}

}  // namespace core::doc
