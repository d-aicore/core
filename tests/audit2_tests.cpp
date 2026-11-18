// Testes da segunda auditoria externa de 2026-11-16 (sweep por analise de codigo: IPC cliente-motor, sync do
// documento, espaco em disco do export, autosave estragado). Cada teste repete o caso do relatorio.
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include "support/check.hpp"
#include "autosave.hpp"
#include "engine_protocol.hpp"
#include "sync_retry.hpp"
#include "core/project.hpp"
#include "core/render_job.hpp"
#include "core/server.hpp"
#include "io/wav.hpp"

namespace fs = std::filesystem;
using core::json;
using core::client::EngineLine;
using core::client::readEngineLine;
using core::client::SyncRetry;

TEST("auditoria 2, bug 1: uma linha do motor com tipos errados nunca lanca no cliente") {
  // O que o cliente fazia antes com estas linhas: json::value com o tipo errado lanca (e lancava na thread de leitura,
  // o que fecha a app com std::terminate).
  CHECK_THROWS(json::parse(R"({"id":1,"ok":false,"error":42})").value("error", std::string("Engine error.")));
  CHECK_THROWS(json::parse(R"({"id":1,"ok":"yes"})").value("ok", false));

  EngineLine r = readEngineLine(R"({"id":7,"ok":false,"error":42})");
  CHECK(r.kind == EngineLine::Kind::Reply);
  CHECK_EQ(r.id, (int64_t)7);
  CHECK(!r.ok);
  CHECK_EQ(r.error, std::string("42"));

  r = readEngineLine(R"({"id":8,"ok":"yes","result":{"a":1}})");
  CHECK(r.kind == EngineLine::Kind::Reply);
  CHECK(!r.ok);  // so "ok": true e sucesso
  CHECK_EQ(r.error, std::string("Engine error."));

  r = readEngineLine(R"({"id":9,"ok":true,"result":{"a":1}})");
  CHECK(r.ok);
  CHECK_EQ(r.result["a"].get<int>(), 1);
  r = readEngineLine(R"({"id":10,"ok":true})");
  CHECK(r.ok);
  CHECK(r.result.is_null());

  r = readEngineLine(R"({"id":"x","error":{"why":"bad"}})");
  CHECK(r.kind == EngineLine::Kind::NoId);
  CHECK(r.error.find("bad") != std::string::npos);
  r = readEngineLine(R"({"id":1.5,"ok":true})");
  CHECK(r.kind == EngineLine::Kind::NoId);
  r = readEngineLine(R"({"error":5})");
  CHECK(r.kind == EngineLine::Kind::NoId);
  CHECK_EQ(r.error, std::string("5"));

  CHECK(readEngineLine("[1,2]").kind == EngineLine::Kind::NotJson);
  CHECK(readEngineLine("not json {").kind == EngineLine::Kind::NotJson);
  CHECK(readEngineLine(std::string("\xff\xfe", 2)).kind == EngineLine::Kind::NotJson);
  CHECK(readEngineLine(R"({"event":"ready","protocol":1})").kind == EngineLine::Kind::Ready);
  CHECK(readEngineLine(R"({"event":7})").kind == EngineLine::Kind::Event);
  // Um erro que nao e texto aparece como JSON.
  r = readEngineLine(R"({"id":3,"ok":false,"error":["a"]})");
  CHECK_EQ(r.error, std::string("[\"a\"]"));
}

TEST("auditoria 2, ponto 4: uma resposta sem id valido termina a sessao logo, com o motivo") {
  // Antes: so ficava no registo, e o comando a espera so falhava no timeout (10 s), sem dizer porque.
  using core::client::endsSession;
  using core::client::endReason;
  for (const char* line : {R"({"id":"1","ok":false,"error":{"msg":"x"}})", R"({"ok":false,"error":"Bad JSON: x"})",
                           R"({"id":null,"ok":true})", R"({"id":2.5,"ok":true,"result":1})"}) {
    const EngineLine r = readEngineLine(line);
    CHECK(endsSession(r));
    CHECK(endReason(r).find("protocol") != std::string::npos);
  }
  CHECK(endReason(readEngineLine(R"({"ok":false,"error":"Bad JSON: x"})")).find("Bad JSON: x") != std::string::npos);
  // O que o protocolo permite continua a nao terminar nada.
  for (const char* line : {R"({"id":1,"ok":false,"error":42})", R"({"id":1,"ok":true})", R"({"event":"state"})",
                           R"({"event":"ready"})", "isto nao e JSON", "[1]"})
    CHECK(!endsSession(readEngineLine(line)));
}

TEST("auditoria 2, bug 2: um project.load sem resposta volta a ser enviado sozinho, com uma revisao nova") {
  SyncRetry retry;
  int delay = -1;
  // Sem resposta a tempo: tenta outra vez, cada vez mais tarde, e para ao fim de 5.
  for (int expected : {500, 1000, 2000, 4000, 8000}) {
    CHECK(retry.failed(true, delay) == SyncRetry::After::Retry);
    CHECK_EQ(delay, expected);
  }
  CHECK(retry.failed(true, delay) == SyncRetry::After::WaitForEdit);
  // Uma edicao (ou uma sessao nova do motor) recomeca a contagem.
  retry.reset();
  CHECK(retry.failed(true, delay) == SyncRetry::After::Retry);
  CHECK_EQ(delay, 500);
  retry.succeeded();
  CHECK_EQ(retry.failures(), 0);
  // O que nao melhora sozinho (recusado, mensagem grande demais) espera pela proxima edicao, como antes.
  CHECK(retry.failed(false, delay) == SyncRetry::After::WaitForEdit);

  // Porque a revisao tem de ser nova: o motor pode ter aceitado a antiga tarde, e recusa-a outra vez como "stale".
  core::Server server(nullptr, 48000);
  const json project = {{"format", core::kProjectFormat}, {"version", 2}, {"bpm", 120}, {"lengthTicks", 960},
                        {"masterGainDb", 0}, {"tracks", json::array()}};
  CHECK(server.handle({{"cmd", "project.load"}, {"args", {{"project", project}, {"revision", 5}}}})["ok"].get<bool>());
  CHECK(!server.handle({{"cmd", "project.load"}, {"args", {{"project", project}, {"revision", 5}}}})["ok"].get<bool>());
  CHECK(server.handle({{"cmd", "project.load"}, {"args", {{"project", project}, {"revision", 6}}}})["ok"].get<bool>());
}

namespace {
fs::space_info spaceOf(uint64_t capacity, uint64_t available) {
  fs::space_info s{};
  s.capacity = capacity;
  s.free = available;
  s.available = available;
  return s;
}
}  // namespace

TEST("auditoria 2, bug 3: o espaco do export conta os mono como mono e cada volume por si") {
  const int64_t frames = 48000 * 60 * 5;  // 5 minutos
  const uint64_t mono = core::exportFileBytes(frames, 24, true), stereo = core::exportFileBytes(frames, 24, false);
  CHECK_EQ(mono, (uint64_t)44 + (uint64_t)frames * 3);
  CHECK_EQ(stereo, (uint64_t)44 + (uint64_t)frames * 6);
  const uint64_t margin = 16u << 20;

  // Dois mono na mesma pasta, com espaco para eles mas nao para dois estereo (o calculo antigo recusava).
  std::vector<core::RenderOutput> outs = {{"/disco/a.wav", -1, false, true}, {"/disco/b.wav", 0, false, true}};
  const uint64_t room = 2 * mono + margin + 1024;
  CHECK(room < 2 * stereo + margin);  // antes: "Not enough disk space"
  auto oneDisk = [&](const fs::path&, fs::space_info& s) { s = spaceOf(1ull << 40, room); return true; };
  core::checkExportSpace(outs, frames, 24, oneDisk);  // nao lanca
  // Um estereo a mais ja nao cabe.
  outs.push_back({"/disco/c.wav", -1, false, false});
  CHECK_THROWS(core::checkExportSpace(outs, frames, 24, oneDisk));

  // Dois volumes: o primeiro tem espaco de sobra, o segundo nao. Antes so se via o do primeiro ficheiro.
  std::vector<core::RenderOutput> two = {{"/grande/mix.wav", -1}, {"/pequeno/stem.wav", 0}};
  auto twoDisks = [&](const fs::path& folder, fs::space_info& s) {
    s = folder == fs::path("/grande") ? spaceOf(1ull << 40, 1ull << 39) : spaceOf(1ull << 30, stereo / 2);
    return true;
  };
  bool named = false;
  try { core::checkExportSpace(two, frames, 24, twoDisks); }
  catch (const std::runtime_error& e) { named = std::string(e.what()).find("pequeno") != std::string::npos; }
  CHECK(named);

  // Duas pastas no mesmo volume somam-se: cada ficheiro cabe sozinho, os dois juntos nao.
  std::vector<core::RenderOutput> shared = {{"/d/um/a.wav", -1}, {"/d/dois/b.wav", 0}};
  auto sameDisk = [&](const fs::path&, fs::space_info& s) { s = spaceOf(1ull << 40, stereo + margin + 1024); return true; };
  CHECK_THROWS(core::checkExportSpace(shared, frames, 24, sameDisk));
  shared.pop_back();
  core::checkExportSpace(shared, frames, 24, sameDisk);

  // Um volume que nao responde nao bloqueia o export (a escrita e que diz).
  core::checkExportSpace(two, frames, 24, [](const fs::path&, fs::space_info&) { return false; });
}

TEST("auditoria 2, observacao: um autosave estragado diz que esta estragado, sem excecao do JSON") {
  const fs::path dir = fs::temp_directory_path() / "core-audit2-tests";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path file = dir / "broken.core-autosave";
  { std::ofstream(file, std::ios::binary) << "{\"format\":\"core-autosave\",\"document\":{"; }
  std::string message;
  try { (void)core::doc::readAutosave(file); } catch (const std::runtime_error& e) { message = e.what(); }
  CHECK(message.find("damaged") != std::string::npos);
  { std::ofstream(file, std::ios::binary | std::ios::trunc) << "[1,2,3]"; }
  message.clear();
  try { (void)core::doc::readAutosave(file); } catch (const std::runtime_error& e) { message = e.what(); }
  CHECK(message.find("not an autosave") != std::string::npos);
  // E a lista de autosaves deixa-o de fora (ja era assim).
  CHECK(core::doc::findAutosaves(dir, "").empty());
}

// Perda de dados (2026-11-18), D2: depois de um crash o Windows pode dar o id do Core que morreu a outro programa.
// A regra antiga ("ha um processo vivo com este id") tomava-o pela sessao ainda a correr e a recuperacao nao era
// oferecida. Agora: so se esse processo for o proprio Core E ja existia quando o autosave foi escrito.
TEST("dados D2: o autosave de um Core que morreu e oferecido mesmo que o id dele ja seja de outro processo") {
  using core::doc::autosaveWriterRunning;
  using core::doc::ProcessProbe;
  const int64_t savedAt = 1'700'000'000'000;  // quando o autosave foi escrito
  const std::string own = "C:\\Program Files\\Core\\Core.exe";
  struct Alive { std::string image; int64_t createdAtMs; };
  const auto with = [](const Alive& a) { return [a](int64_t) { return ProcessProbe{true, a.image, a.createdAtMs}; }; };
  const auto nobody = [](int64_t) { return ProcessProbe{}; };

  // O id e de um processo vivo que NAO e o Core: a sessao que escreveu morreu -> oferecer.
  CHECK(!autosaveWriterRunning(4242, savedAt, own, with({"C:\\Windows\\explorer.exe", savedAt - 60'000})));
  // E do Core, e ja corria quando o autosave foi escrito: e outra janela a trabalhar -> nao oferecer.
  CHECK(autosaveWriterRunning(4242, savedAt, own, with({"C:\\Program Files\\Core\\Core.exe", savedAt - 60'000})));
  // E do Core mas nasceu DEPOIS do autosave: nao foi ele que o escreveu (id reutilizado por um Core novo) -> oferecer.
  CHECK(!autosaveWriterRunning(4242, savedAt, own, with({"C:\\Program Files\\Core\\Core.exe", savedAt + 5'000})));
  // Nao ha processo com esse id -> oferecer (ja era assim).
  CHECK(!autosaveWriterRunning(4242, savedAt, own, nobody));

  // O nome compara-se pelo executavel, sem olhar a pasta nem a maiusculas; outro nome que acabe igual nao e o Core.
  CHECK(autosaveWriterRunning(4242, savedAt, own, with({"d:/outra pasta/CORE.EXE", savedAt - 1})));
  CHECK(autosaveWriterRunning(4242, savedAt, "core.exe", with({"Core.exe", savedAt})));  // criado no mesmo ms: ja existia
  CHECK(!autosaveWriterRunning(4242, savedAt, own, with({"C:\\Program Files\\Core\\NotCore.exe", savedAt - 60'000})));
  CHECK(!autosaveWriterRunning(4242, savedAt, own, with({"C:\\Program Files\\Core\\core-engine.exe", savedAt - 60'000})));
  // Na duvida oferece-se (um autosave oferecido a mais nao perde nada; um que nao se oferece perde o trabalho):
  // sem nome, sem instante de criacao, sem saber o nome do proprio Core, ou um autosave sem data.
  CHECK(!autosaveWriterRunning(4242, savedAt, own, with({"", savedAt - 60'000})));
  CHECK(!autosaveWriterRunning(4242, savedAt, own, with({"C:\\Program Files\\Core\\Core.exe", 0})));
  CHECK(!autosaveWriterRunning(4242, savedAt, "", with({"", savedAt - 60'000})));
  CHECK(!autosaveWriterRunning(4242, 0, own, with({"C:\\Program Files\\Core\\Core.exe", savedAt - 60'000})));
  // Um id que nao e de processo nenhum (0: o self-test escreve assim) nem chega a perguntar.
  bool asked = false;
  CHECK(!autosaveWriterRunning(0, savedAt, own, [&](int64_t) { asked = true; return ProcessProbe{true, own, savedAt - 1}; }));
  CHECK(!asked);
}

// BUG-SWEEP-02 (export working-file ownership, sweep of 2026-11-18): the export wrote to a fixed
// "<target>.part" opened with truncation, and its cleanup removed EVERY listed .part path. A successful
// export consumed a preexisting partial export (a retained recovery file), and two engine processes
// exporting the same target shared one working path. Each test below failed before the fix.
namespace {
fs::path exportPartDir(const std::string& name) {
  const fs::path d = fs::temp_directory_path() / "core-audit2-exportpart" / name;
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}
std::string readFileBytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
void seedPreciousPart(const std::string& target) {
  std::ofstream(target + ".part", std::ios::binary) << "valuable previous partial export";
}
std::vector<std::string> partNamesIn(const fs::path& dir) {
  std::vector<std::string> names;
  for (const auto& e : fs::directory_iterator(dir))
    if (e.is_regular_file() && e.path().extension() == ".part") names.push_back(e.path().filename().string());
  std::sort(names.begin(), names.end());
  return names;
}
core::json waitForJob(core::RenderJobs& jobs, uint64_t id) {
  core::json status;
  for (int i = 0; i < 10000; ++i) {
    status = jobs.status(id);
    const std::string s = status.value("state", "");
    if (s == "succeeded" || s == "failed" || s == "cancelled") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return status;
}
core::RenderJobRequest silentRequest(const std::string& target) {
  core::RenderJobRequest r;
  r.path = target;
  r.project.lengthTicks = 960;
  r.tailSeconds = 0;
  return r;
}
}  // namespace

TEST("BUG-SWEEP-02: a successful export never consumes a preexisting .part and cleans up only its own working file") {
  const fs::path dir = exportPartDir("success");
  const std::string target = (dir / "mix.wav").string();
  seedPreciousPart(target);
  core::RenderJobs jobs;
  const uint64_t id = jobs.start(silentRequest(target))["jobId"].get<uint64_t>();
  const core::json status = waitForJob(jobs, id);
  CHECK_EQ(status.value("state", ""), std::string("succeeded"));
  CHECK(fs::exists(target));
  // The retained partial export survives byte-identical (before the fix it was truncated, then renamed away).
  CHECK(fs::exists(target + ".part"));
  CHECK_EQ(readFileBytes(target + ".part"), std::string("valuable previous partial export"));
  // The published file is the export's own WAV, and only the preexisting .part remains in the folder.
  CHECK_EQ(readFileBytes(target).substr(0, 4), std::string("RIFF"));
  CHECK_EQ(partNamesIn(dir).size(), (size_t)1);
}

TEST("BUG-SWEEP-02: a failed or cancelled export keeps the preexisting .part and removes only its own working files") {
  // A clip louder than 0 dBFS: the level check fails AFTER the working file was written and closed.
  {
    const fs::path dir = exportPartDir("failed-loud");
    const std::string target = (dir / "mix.wav").string();
    const std::vector<float> loud(24000, 2.0f);
    core::writeWav((dir / "loud.wav").string(), loud, loud, 48000, 32);
    seedPreciousPart(target);
    core::RenderJobs jobs;
    core::RenderJobRequest r = silentRequest(target);
    core::Track t; t.id = "audio1"; t.instrument = "audio";
    core::AudioClip c; c.path = (dir / "loud.wav").string(); c.durationSeconds = 0.5;
    t.audioClips.push_back(c);
    r.project.tracks.push_back(t);
    r.assets = std::make_shared<core::AssetStore>();
    const uint64_t id = jobs.start(r)["jobId"].get<uint64_t>();
    const core::json status = waitForJob(jobs, id);
    CHECK_EQ(status.value("state", ""), std::string("failed"));
    CHECK(status.value("error", "").find("0 dBFS") != std::string::npos);
    CHECK(!fs::exists(target));  // nothing published
    CHECK(fs::exists(target + ".part"));
    CHECK_EQ(readFileBytes(target + ".part"), std::string("valuable previous partial export"));
    CHECK_EQ(partNamesIn(dir).size(), (size_t)1);  // the job's own working file was removed
  }
  // Audio that cannot be read: the job fails before any working file is reserved; the cleanup must still
  // leave the retained .part alone (before the fix it removed every listed .part).
  {
    const fs::path dir = exportPartDir("failed-missing");
    const std::string target = (dir / "mix.wav").string();
    seedPreciousPart(target);
    core::RenderJobs jobs;
    core::RenderJobRequest r = silentRequest(target);
    core::Track t; t.id = "audio1"; t.instrument = "audio";
    core::AudioClip c; c.path = (dir / "gone.wav").string(); c.durationSeconds = 0.5;
    t.audioClips.push_back(c);
    r.project.tracks.push_back(t);
    r.assets = std::make_shared<core::AssetStore>();
    const uint64_t id = jobs.start(r)["jobId"].get<uint64_t>();
    const core::json status = waitForJob(jobs, id);
    CHECK_EQ(status.value("state", ""), std::string("failed"));
    CHECK(status.value("error", "").find("Missing audio") != std::string::npos);
    CHECK(!fs::exists(target));
    CHECK(fs::exists(target + ".part"));
    CHECK_EQ(readFileBytes(target + ".part"), std::string("valuable previous partial export"));
    CHECK_EQ(partNamesIn(dir).size(), (size_t)1);
  }
  // Cancelled while the working files exist (the hook cancels at the publish decision point).
  {
    const fs::path dir = exportPartDir("cancelled");
    const std::string target = (dir / "mix.wav").string();
    seedPreciousPart(target);
    core::RenderJobs jobs;
    jobs.testHooks.beforePublish = [&](uint64_t jobId) { jobs.cancel(jobId); };
    const uint64_t id = jobs.start(silentRequest(target))["jobId"].get<uint64_t>();
    const core::json status = waitForJob(jobs, id);
    CHECK_EQ(status.value("state", ""), std::string("cancelled"));
    CHECK(!fs::exists(target));
    CHECK(fs::exists(target + ".part"));
    CHECK_EQ(readFileBytes(target + ".part"), std::string("valuable previous partial export"));
    CHECK_EQ(partNamesIn(dir).size(), (size_t)1);
  }
}

TEST("BUG-SWEEP-02: two concurrent exports of the same target never share a working path") {
  const fs::path dir = exportPartDir("concurrent");
  const std::string target = (dir / "mix.wav").string();
  seedPreciousPart(target);
  // Both workers meet inside beforePublish: each snapshots the folder while BOTH jobs hold their
  // working files, so the census cannot miss them by timing.
  std::atomic<int> arrived{0}, snapshotted{0};
  std::vector<std::string> seenA, seenB;
  const auto meet = [&] {
    arrived.fetch_add(1);
    for (int i = 0; i < 5000 && arrived.load() < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };
  const auto leave = [&] {
    snapshotted.fetch_add(1);
    for (int i = 0; i < 5000 && snapshotted.load() < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };
  core::RenderJobs jobsA, jobsB;
  jobsA.testHooks.beforePublish = [&](uint64_t) { meet(); seenA = partNamesIn(dir); leave(); };
  jobsB.testHooks.beforePublish = [&](uint64_t) { meet(); seenB = partNamesIn(dir); leave(); };
  const uint64_t idA = jobsA.start(silentRequest(target))["jobId"].get<uint64_t>();
  const uint64_t idB = jobsB.start(silentRequest(target))["jobId"].get<uint64_t>();
  const core::json a = waitForJob(jobsA, idA), b = waitForJob(jobsB, idB);
  // While both jobs held their working files, two DISTINCT names besides the retained .part existed.
  const auto working = [](const std::vector<std::string>& seen) {
    std::vector<std::string> w;
    for (const auto& n : seen) if (n != "mix.wav.part") w.push_back(n);
    return w;
  };
  CHECK(working(seenA).size() >= (size_t)2);
  CHECK(working(seenB).size() >= (size_t)2);
  // The retained partial export is untouched and the target was published (at least one job succeeded;
  // two publishes of one target race by design, the last one in place wins).
  CHECK(fs::exists(target + ".part"));
  CHECK_EQ(readFileBytes(target + ".part"), std::string("valuable previous partial export"));
  CHECK(fs::exists(target));
  CHECK(a.value("state", "") == "succeeded" || b.value("state", "") == "succeeded");
  CHECK_EQ(partNamesIn(dir).size(), (size_t)1);
}

TEST("BUG-SWEEP-02: working-file reservations are unique, exclusive and never truncate an existing candidate") {
  const fs::path dir = exportPartDir("reserve");
  const std::string target = (dir / "mix.wav").string();
  const std::string a = core::reserveRenderWorkingFile(target);
  const std::string b = core::reserveRenderWorkingFile(target);
  CHECK(a != b);
  CHECK(fs::exists(a));
  CHECK(fs::exists(b));
  CHECK(a != target + ".part");
  CHECK(b != target + ".part");
  CHECK_EQ(a.compare(0, target.size(), target), 0);
  CHECK_EQ(a.substr(a.size() - 5), std::string(".part"));
  // A candidate that already exists (a retained file) is never reused or truncated.
  { std::ofstream(a, std::ios::binary) << "retained recovery bytes"; }
  const std::string c = core::reserveRenderWorkingFile(target);
  CHECK(c != a);
  CHECK(c != b);
  CHECK_EQ(readFileBytes(a), std::string("retained recovery bytes"));
  // Many threads reserving for the SAME target in this process: no two share a path (the exclusive
  // create plus the pid in the name give the same guarantee across processes).
  std::mutex m;
  std::vector<std::string> paths;
  std::vector<std::thread> ts;
  for (int t = 0; t < 8; ++t) ts.emplace_back([&] {
    for (int i = 0; i < 25; ++i) {
      const std::string p = core::reserveRenderWorkingFile(target);
      std::lock_guard<std::mutex> g(m);
      paths.push_back(p);
    }
  });
  for (auto& th : ts) th.join();
  CHECK_EQ(paths.size(), (size_t)200);
  std::sort(paths.begin(), paths.end());
  CHECK(std::adjacent_find(paths.begin(), paths.end()) == paths.end());
  for (const auto& p : paths) CHECK(fs::exists(p));
  // The reservations belong to the caller: removing them is the caller's cleanup, nothing else goes.
  for (const auto& p : paths) fs::remove(p);
  fs::remove(a);
  fs::remove(b);
  fs::remove(c);
  CHECK_EQ(partNamesIn(dir).size(), (size_t)0);
}

CORE_TEST_MAIN
