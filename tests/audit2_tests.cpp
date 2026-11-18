// Testes da segunda auditoria externa de 2026-11-16 (sweep por analise de codigo: IPC cliente-motor, sync do
// documento, espaco em disco do export, autosave estragado). Cada teste repete o caso do relatorio.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "support/check.hpp"
#include "autosave.hpp"
#include "engine_protocol.hpp"
#include "sync_retry.hpp"
#include "core/project.hpp"
#include "core/render_job.hpp"
#include "core/server.hpp"

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

// BUG-SWEEP-03 (2026-11-18): a failed export must keep every recoverable copy of the user's file and say so.
// The old undo dropped the restore error: the original sat in a .previous backup nobody was told about, the
// target was left missing, and the app still said "No existing file was changed." The injection: a directory
// obstructs the target AFTER the original was moved aside, so both the placement and the restore fail.
TEST("BUG-SWEEP-03: a failed publish keeps the previous file and names it in the error") {
  const fs::path dir = fs::temp_directory_path() / "core-sweep03-publish";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path target = dir / "mix.wav";
  { std::ofstream(target, std::ios::binary) << "THE USER'S OLD EXPORT"; }
  { std::ofstream(target.string() + ".part", std::ios::binary) << "THE NEW EXPORT"; }
  std::vector<core::RenderOutput> outputs{{target.string(), -1}};
  int published = -1;
  bool threw = false;
  std::string message;
  try {
    core::publishRenderOutputs(outputs, published, [&] {
      fs::remove(target);
      fs::create_directory(target);  // the placement cannot rename over a folder, nor can the restore
      throw std::runtime_error("synthetic placement failure");
    });
  } catch (const std::exception& e) { threw = true; message = e.what(); }
  CHECK(threw);
  CHECK_EQ(published, 0);            // no new file was left in place
  CHECK(fs::is_directory(target));   // the obstruction stays: the restore could not put the file back
  const fs::path backup = dir / "mix.wav.previous";
  CHECK(fs::exists(backup));         // the original was kept, never deleted
  {
    std::ifstream in(backup, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(bytes, std::string("THE USER'S OLD EXPORT"));
  }
  // The error must name the kept copy so the user can recover it (before the fix it said nothing about it).
  CHECK(message.find("mix.wav.previous") != std::string::npos);
  fs::remove_all(dir);
}

TEST("BUG-SWEEP-03: a failed export job reports the rollback and the kept backups") {
  const fs::path dir = fs::temp_directory_path() / "core-sweep03-job";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path target = dir / "mix.wav";
  { std::ofstream(target, std::ios::binary) << "THE USER'S OLD EXPORT"; }
  core::RenderJobs jobs;
  jobs.testHooks.beforePlace = [&](uint64_t) { fs::remove(target); fs::create_directory(target); };
  core::RenderJobRequest req;
  req.project.bpm = 120;
  req.project.lengthTicks = 480;  // one beat of silence: a quick render
  req.sampleRate = 48000;
  req.bits = 24;
  req.tailSeconds = 0;
  req.path = target.string();
  const json started = jobs.start(req);
  const uint64_t id = started["jobId"].get<uint64_t>();
  json st = started;
  for (int i = 0; i < 300; ++i) {
    st = jobs.status(id);
    const std::string s = st.value("state", "");
    if (s == "failed" || s == "succeeded" || s == "cancelled") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  CHECK_EQ(st.value("state", std::string()), std::string("failed"));
  CHECK_EQ(st.value("published", 0), 0);
  CHECK(fs::is_directory(target));
  const fs::path backup = dir / "mix.wav.previous";
  CHECK(fs::exists(backup));
  {
    std::ifstream in(backup, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(bytes, std::string("THE USER'S OLD EXPORT"));
  }
  // The status must report the rollback outcome and the kept backup: the app conditions its message on them.
  CHECK(st.value("rollbackAttempted", false));
  CHECK(st.contains("backups") && st["backups"].is_array() && st["backups"].size() == 1);
  if (st.contains("backups") && st["backups"].is_array() && st["backups"].size() == 1)
    CHECK_EQ(st["backups"][0].get<std::string>(), target.string() + ".previous");
  CHECK(st.contains("restored") && st["restored"].is_array() && st["restored"].empty());
  CHECK(st.value("error", std::string()).find("mix.wav.previous") != std::string::npos);
  fs::remove_all(dir);
}

TEST("BUG-SWEEP-03: a publish that can restore puts the old file back and reports it") {
  const fs::path dir = fs::temp_directory_path() / "core-sweep03-restore";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path target = dir / "mix.wav";
  { std::ofstream(target, std::ios::binary) << "THE USER'S OLD EXPORT"; }
  core::RenderJobs jobs;
  jobs.testHooks.beforePlace = [](uint64_t) { throw std::runtime_error("synthetic failure before placement"); };
  core::RenderJobRequest req;
  req.project.bpm = 120;
  req.project.lengthTicks = 480;
  req.sampleRate = 48000;
  req.bits = 24;
  req.tailSeconds = 0;
  req.path = target.string();
  const json started = jobs.start(req);
  const uint64_t id = started["jobId"].get<uint64_t>();
  json st = started;
  for (int i = 0; i < 300; ++i) {
    st = jobs.status(id);
    const std::string s = st.value("state", "");
    if (s == "failed" || s == "succeeded" || s == "cancelled") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  CHECK_EQ(st.value("state", std::string()), std::string("failed"));
  CHECK_EQ(st.value("published", 0), 0);
  CHECK(fs::is_regular_file(target));  // the original is back at its path, byte for byte
  {
    std::ifstream in(target, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(bytes, std::string("THE USER'S OLD EXPORT"));
  }
  CHECK(!fs::exists(dir / "mix.wav.previous"));  // nothing stranded: the restore used the copy up
  CHECK(st.value("rollbackAttempted", false));
  CHECK(st.contains("backups") && st["backups"].is_array() && st["backups"].empty());
  CHECK(st.contains("restored") && st["restored"].is_array() && st["restored"].size() == 1);
  if (st.contains("restored") && st["restored"].is_array() && st["restored"].size() == 1)
    CHECK_EQ(st["restored"][0].get<std::string>(), target.string());
  // Nothing was kept aside, so the error must not invent a kept copy.
  CHECK(st.value("error", std::string()).find(".previous") == std::string::npos);
  fs::remove_all(dir);
}

TEST("BUG-SWEEP-03: the rollback report names what was restored and what was kept") {
  const fs::path dir = fs::temp_directory_path() / "core-sweep03-mixed";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path mix = dir / "mix.wav", stem = dir / "stem.wav";
  { std::ofstream(mix, std::ios::binary) << "OLD MIX"; }
  { std::ofstream(stem, std::ios::binary) << "OLD STEM"; }
  { std::ofstream(mix.string() + ".part", std::ios::binary) << "NEW MIX"; }
  { std::ofstream(stem.string() + ".part", std::ios::binary) << "NEW STEM"; }
  const std::vector<core::RenderOutput> outputs{{mix.string(), -1}, {stem.string(), 0}};
  core::PublishRollback rb;
  int published = -1;
  bool threw = false;
  try {
    core::publishRenderOutputs(outputs, published, [&] { fs::create_directory(stem); }, &rb);
  } catch (const std::exception&) { threw = true; }
  CHECK(threw);
  CHECK_EQ(published, 0);
  CHECK(rb.attempted);
  // The first file went back to its path; the second's old copy stayed aside, named for recovery.
  CHECK_EQ(rb.restored.size(), (size_t)1);
  if (rb.restored.size() == 1) CHECK_EQ(rb.restored[0], mix.string());
  CHECK_EQ(rb.backups.size(), (size_t)1);
  if (rb.backups.size() == 1) CHECK_EQ(rb.backups[0], stem.string() + ".previous");
  {
    std::ifstream in(mix, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(bytes, std::string("OLD MIX"));
  }
  {
    std::ifstream in(stem.string() + ".previous", std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(bytes, std::string("OLD STEM"));
  }
  fs::remove_all(dir);
}

TEST("BUG-SWEEP-03: a successful publish reports no rollback and takes the new files") {
  const fs::path dir = fs::temp_directory_path() / "core-sweep03-success";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path target = dir / "mix.wav";
  { std::ofstream(target, std::ios::binary) << "OLD MIX"; }
  { std::ofstream(target.string() + ".part", std::ios::binary) << "NEW MIX"; }
  const std::vector<core::RenderOutput> outputs{{target.string(), -1}};
  core::PublishRollback rb;
  int published = -1;
  core::publishRenderOutputs(outputs, published, {}, &rb);
  CHECK_EQ(published, 1);
  CHECK(!rb.attempted);
  CHECK(rb.restored.empty());
  CHECK(rb.backups.empty());
  CHECK(!fs::exists(target.string() + ".previous"));  // success: the old copy is used up, not stranded
  {
    std::ifstream in(target, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(bytes, std::string("NEW MIX"));
  }
  fs::remove_all(dir);
}

CORE_TEST_MAIN
