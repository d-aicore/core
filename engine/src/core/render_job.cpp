#include "render_job.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include "engine.hpp"
#include "graph.hpp"
#include "../io/wav.hpp"
#include "../dsp/finite.hpp"
#include <cstdio>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace core {

namespace fs = std::filesystem;

namespace {
// Publishing: the worker has claimed the end of the job (renaming the .part over the target). It is the
// only way from Running to Succeeded, and cancel() can no longer win once it is taken; it is reported as
// "running" (it lasts a rename), so the app never sees "cancelling" followed by "succeeded".
enum class State { Queued, Running, Cancelling, Publishing, Succeeded, Cancelled, Failed };
const char* name(State s) {
  switch (s) {
    case State::Queued: return "queued";
    case State::Running: return "running";
    case State::Publishing: return "running";
    case State::Cancelling: return "cancelling";
    case State::Succeeded: return "succeeded";
    case State::Cancelled: return "cancelled";
    case State::Failed: return "failed";
  }
  return "?";
}
bool finished(State s) { return s == State::Succeeded || s == State::Cancelled || s == State::Failed; }
fs::path utf8(const std::string& s) { return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size())); }
bool sameTarget(const fs::path& a, const fs::path& b) {
  const auto x = fs::absolute(a).lexically_normal();
  const auto y = fs::absolute(b).lexically_normal();
#ifdef _WIN32
  // Match Windows path casing without depending on the process's ANSI codepage/locale.
  return CompareStringOrdinal(x.c_str(), -1, y.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
  return x == y;
#endif
}
constexpr int kBlock = 4096;  // frames per step: cancel is checked this often (~85 ms of audio)
}

std::string reserveRenderWorkingFile(const std::string& target) {
  // BUG-SWEEP-02: the working file belongs to the operation that created it. A fixed "<target>.part"
  // was opened with truncation (consuming a retained partial export) and shared by every process
  // exporting the same target. The name carries the process id and a process-wide sequence, and the
  // exclusive create IS the reservation: an existing candidate is never opened, only skipped.
  static std::atomic<uint64_t> sequence{0};
#ifdef _WIN32
  const uint64_t pid = GetCurrentProcessId();
#else
  const uint64_t pid = (uint64_t)getpid();
#endif
  const fs::path folder = utf8(target).parent_path();
  if (!folder.empty()) { std::error_code e; fs::create_directories(folder, e); }  // as WavWriter does; the create below reports a blocked folder
  for (int attempt = 0; attempt < 4096; ++attempt) {
    const std::string candidate = target + "." + std::to_string(pid) + "-" + std::to_string(sequence.fetch_add(1)) + ".part";
    const fs::path path = utf8(candidate);
#ifdef _WIN32
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return candidate; }
    const DWORD err = GetLastError();
    if (err != ERROR_FILE_EXISTS && err != ERROR_ALREADY_EXISTS)
      throw std::runtime_error("Cannot reserve a working file for " + target + " (error " + std::to_string(err) + ").");
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0) { ::close(fd); return candidate; }
    if (errno != EEXIST)
      throw std::runtime_error("Cannot reserve a working file for " + target + " (" + std::strerror(errno) + ").");
#endif
    // Taken (a retained file or another operation's reservation): the next name, never this file.
  }
  throw std::runtime_error("Cannot reserve a working file for " + target + ": too many name collisions.");
}

void validateRenderOutputs(const std::vector<RenderOutput>& outputs) {
  std::error_code e;
  std::vector<fs::path> targets;
  for (const auto& out : outputs) {
    if (out.path.empty()) throw std::runtime_error("Every file needs a 'path'.");
    const fs::path target = utf8(out.path);
    const fs::path folder = target.has_parent_path() ? target.parent_path() : fs::current_path();
    if (!fs::is_directory(folder, e)) throw std::runtime_error("The folder " + folder.string() + " does not exist.");
    for (const auto& t : targets)
      if (sameTarget(t, target)) throw std::runtime_error("Two files have the same path: " + out.path + ".");
    targets.push_back(target);
  }
  // A file's ".part" (written while it renders) must not be another file of the same export (audit 2026-11-07).
  for (const auto& t : targets)
    for (const auto& u : targets)
      if (sameTarget(fs::path(t.native() + utf8(".part").native()), u))
        throw std::runtime_error("One file is another one's working copy (.part): " + u.string() + ".");
}

void publishRenderOutputs(const std::vector<RenderOutput>& outputs, int& published, const std::function<void()>& beforePlace,
                          const std::vector<std::string>& workingPaths, PublishRollback* rollback) {
  const size_t count = outputs.size();
  if (!workingPaths.empty() && workingPaths.size() != count)
    throw std::runtime_error("The working files do not match the outputs.");
  // The reserved working file of each output; without one, the legacy "<path>.part" (the synchronous CLI).
  auto partOf = [&](size_t f) { return workingPaths.empty() ? utf8(outputs[f].path + ".part") : utf8(workingPaths[f]); };
  published = 0;
  // Reserve backups before moving anything; all paths use the same alias rules.
  std::vector<fs::path> asides(count);
  std::vector<std::string> asideNames(count);  // the same paths as text, for the rollback report
  {
    std::vector<fs::path> taken;
    for (size_t f = 0; f < count; ++f) { taken.push_back(utf8(outputs[f].path).lexically_normal()); taken.push_back(partOf(f).lexically_normal()); }
    for (size_t f = 0; f < count; ++f) {
      for (int k = 1;; ++k) {
        const std::string name = outputs[f].path + ".previous" + (k == 1 ? std::string() : "-" + std::to_string(k));
        const fs::path candidate = utf8(name);
        std::error_code ignored;
        bool clash = fs::exists(candidate, ignored);
        for (const auto& t : taken) clash = clash || sameTarget(t, candidate);
        if (!clash) { asides[f] = candidate; asideNames[f] = name; taken.push_back(candidate.lexically_normal()); break; }
      }
    }
  }
  std::vector<char> aside(count, 0), placed(count, 0);
  PublishRollback rb;
  auto undo = [&] {
    rb.attempted = true;
    int left = 0;  // new files that could not be taken back (reported as "published")
    for (size_t f = 0; f < count; ++f) {
      std::error_code ignored;
      if (placed[f]) { fs::remove(utf8(outputs[f].path), ignored); if (ignored) ++left; }
      if (aside[f]) {
        fs::rename(asides[f], utf8(outputs[f].path), ignored);
        // A restore that fails leaves the old copy where it is: kept and named, never deleted (BUG-SWEEP-03).
        if (ignored) rb.backups.push_back(asideNames[f]);
        else rb.restored.push_back(outputs[f].path);
      }
    }
    published = left;
  };
  try {
    for (size_t f = 0; f < count; ++f) {
      const fs::path target = utf8(outputs[f].path);
      std::error_code e;
      if (fs::is_directory(target, e)) throw std::runtime_error("Cannot replace " + outputs[f].path + ": it is a folder.");
      if (!fs::exists(target, e)) continue;
      fs::rename(target, asides[f], e);
      if (e) { throw std::runtime_error("Cannot replace " + outputs[f].path + ": " + e.message()); }
      aside[f] = 1;
    }
    if (beforePlace) beforePlace();
    for (size_t f = 0; f < count; ++f) {
      std::error_code e;
      fs::rename(partOf(f), utf8(outputs[f].path), e);
      if (e) { throw std::runtime_error("Cannot replace " + outputs[f].path + ": " + e.message()); }
      placed[f] = 1;
    }
  } catch (const std::exception& ex) {
    undo();
    if (rollback) *rollback = rb;
    // The kept copies go into the error itself: the CLI and any other caller can name them too.
    std::string msg = ex.what();
    if (!rb.backups.empty()) {
      msg += rb.backups.size() == 1 ? " The previous file was kept as a copy: " : " The previous files were kept as copies: ";
      for (size_t i = 0; i < rb.backups.size(); ++i) msg += (i ? "; " : "") + rb.backups[i];
    }
    throw std::runtime_error(msg);
  } catch (...) { undo(); if (rollback) *rollback = rb; throw; }
  if (rollback) *rollback = rb;
  for (size_t f = 0; f < count; ++f) {
    std::error_code ignored;
    if (aside[f]) fs::remove(asides[f], ignored);
  }
  published = (int)count;
}

struct RenderJobs::Job {
  uint64_t id;
  RenderJobRequest request;
  int64_t frames = 0;  // per file
  int64_t skip = 0;    // song samples played before the region (not written)
  std::atomic<int> file{0};  // the output being rendered
  std::atomic<State> state{State::Queued};
  std::atomic<double> progress{0};
  std::atomic<bool> cancel{false};
  // Written by the worker before the final state is stored.
  std::string error;
  float peak = 0;
  double renderMs = 0;
  int published = 0;  // files put in place (all of them on success; on a failure only those that could not be undone)
  PublishRollback rollback;  // what the undo of a failed publication did: restored files and kept .previous copies
  int threads = 0;  // export workers besides the job's own thread (M4a)
  // Last values reported by takeUpdates().
  State reportedState = State::Queued;
  double reportedProgress = -1;
};

RenderJobs::~RenderJobs() {
  {
    std::lock_guard<std::mutex> g(mutex_);
    if (current_) current_->cancel = true;
  }
  if (worker_.joinable()) worker_.join();
}

bool RenderJobs::busy() const {
  std::lock_guard<std::mutex> g(mutex_);
  return current_ && !finished(current_->state.load());
}

void ExportLevel::add(const float* x, int n) {
  for (int i = 0; i < n; ++i) {
    const float a = std::fabs(x[i]);
    if (!finiteBits(x[i])) broken = true;
    else if (a > peak) peak = a;
  }
}

void ExportLevel::addMono(const float* L, const float* R, int n) {
  for (int i = 0; i < n; ++i) {
    if (!finiteBits(L[i]) || !finiteBits(R[i])) { broken = true; continue; }
    const float a = std::fabs(0.5f * (L[i] + R[i]));
    if (a > peak) peak = a;
  }
}

void checkExportLevel(const ExportLevel& level, const RenderOutput& output) {
  const std::string full = output.path;
  const size_t slash = full.find_last_of("/\\");
  const std::string name = slash == std::string::npos ? full : full.substr(slash + 1);
  if (level.broken)
    throw std::runtime_error("The export of " + name + " has broken samples (NaN or Inf). Nothing was written; an effect or an instrument is unstable.");
  if (!output.withoutMasterGain && level.peak > 1.0f) {
    char db[32];
    std::snprintf(db, sizeof db, "%+.1f", 20 * std::log10((double)level.peak));
    throw std::runtime_error("The export of " + name + " goes over 0 dBFS (peak " + db + " dBFS), so it would clip. Nothing was written: lower the master, or end it with a limiter.");
  }
}

uint64_t exportFileBytes(int64_t frames, int bits, bool mono) {
  return 44 + (uint64_t)std::max<int64_t>(0, frames) * (mono ? 1u : 2u) * (uint64_t)(bits / 8);
}

void checkExportSpace(const std::vector<RenderOutput>& outputs, int64_t frames, int bits, const DiskSpaceQuery& query) {
  constexpr uint64_t kMargin = 16u << 20;  // room left for everything else
  // The bytes each folder gets.
  std::vector<std::pair<fs::path, uint64_t>> folders;
  for (const auto& out : outputs) {
    const fs::path target = utf8(out.path);
    const fs::path folder = target.has_parent_path() ? target.parent_path() : fs::path(".");
    const uint64_t bytes = exportFileBytes(frames, bits, out.mono);
    auto it = std::find_if(folders.begin(), folders.end(), [&](const auto& f) { return f.first == folder; });
    if (it == folders.end()) folders.push_back({folder, bytes});
    else it->second += bytes;
  }
  // The same volume answers the same capacity and free space: those folders share it.
  struct Volume { fs::space_info space; uint64_t bytes = 0; fs::path folder; };
  std::vector<Volume> volumes;
  for (const auto& [folder, bytes] : folders) {
    fs::space_info space{};
    bool known = false;
    if (query) known = query(folder, space);
    else {
      std::error_code e;
      space = fs::space(folder, e);
      known = !e;
    }
    if (!known) continue;  // a folder that cannot be asked: the write itself will say
    auto it = std::find_if(volumes.begin(), volumes.end(), [&](const Volume& v) {
      return v.space.capacity == space.capacity && v.space.available == space.available;
    });
    if (it == volumes.end()) volumes.push_back({space, bytes, folder});
    else it->bytes += bytes;
  }
  for (const auto& v : volumes)
    if (v.space.available < v.bytes + kMargin) {
      const std::u8string name = v.folder.u8string();
      throw std::runtime_error("Not enough disk space: " + std::to_string((v.bytes + (1u << 20) - 1) >> 20) + " MB needed in " +
                               std::string(name.begin(), name.end()) + ", " + std::to_string(v.space.available >> 20) + " MB free.");
    }
}

// S1 (DATA priority): the refusal both export paths share (the app's RenderJobs and the CLI's
// renderProject). A needed plugin that could not load, or that failed while rendering, refuses the whole
// export BEFORE anything is published: the previous export is never replaced by a degraded one (the red
// measurement showed the degraded render byte-identical to the healthy control, published as success).
// Bypassed units are intentional silence and never refuse. The error names each plugin / track / why, so
// the CLI answer (ok:false) and the app job (State::Failed -> describe's error) both surface the reason.
void checkExportPlugins(const Graph& graph) {
  const std::vector<json> missing = graph.missingNeededPlugins();
  const std::vector<json> failed = graph.failedPlugins();
  if (missing.empty() && failed.empty()) return;
  std::vector<std::string> parts;
  for (const auto& m : missing)
    parts.push_back("plugin '" + m.value("plugin", std::string("plugin")) + "' (" + m.value("key", std::string()) +
                    ") did not load: " + m.value("why", std::string("unknown reason")));
  for (const auto& f : failed)
    parts.push_back("plugin '" + f.value("plugin", std::string("plugin")) + "' (" + f.value("key", std::string()) +
                    ") failed during the render: " + f.value("why", std::string("unknown reason")));
  std::string msg = "Export refused: ";
  for (size_t i = 0; i < parts.size(); ++i) msg += (i ? "; " : "") + parts[i];
  msg += ". Nothing was published; the previous export is untouched. Fix or bypass the plugin, then export again.";
  throw std::runtime_error(msg);
}

json RenderJobs::start(RenderJobRequest request) {
  if (request.bits != 24 && request.bits != 32) throw std::runtime_error("bits must be 24 or 32.");
  if (!(request.tailSeconds >= 0 && request.tailSeconds <= 60)) throw std::runtime_error("tailMs must be 0-60000.");
  if (request.outputs.empty()) {
    if (request.path.empty()) throw std::runtime_error("'path' is required.");
    request.outputs.push_back({request.path, -1});
  }
  if (request.path.empty()) request.path = request.outputs.front().path;
  if (request.outputs.size() > 64) throw std::runtime_error("At most 64 files in one export.");
  validateRenderOutputs(request.outputs);
  for (const auto& out : request.outputs)
    if (out.track < -1 || out.track >= (int)request.project.tracks.size()) throw std::runtime_error("No such track for " + out.path + ".");
  // The region: nothing starts after its end (the song is cut there, its tail rings out).
  Project& project = request.project;
  if (request.endTick >= 0) project.lengthTicks = std::min(project.lengthTicks, request.endTick);
  request.endTick = project.lengthTicks;
  if (!(request.startTick >= 0 && request.startTick < request.endTick))
    throw std::runtime_error("The region must start at 0 or later and before its end.");
  // Length and size are known before starting: check them now, not after minutes of work.
  const TempoMap tempo(project.bpm, project.tempoMap, request.sampleRate);
  const int64_t skip = tempo.sampleOfTick(request.startTick);
  const int64_t frames = tempo.sampleOfTick(request.endTick) - skip + (int64_t)std::llround(request.tailSeconds * request.sampleRate);
  if ((uint64_t)frames > WavWriter::maxFrames(request.bits)) throw std::runtime_error("The song is too long for one WAV file (4 GB limit).");
  checkExportSpace(request.outputs, frames, request.bits);

  std::lock_guard<std::mutex> g(mutex_);
  if (current_ && !finished(current_->state.load()))
    throw std::runtime_error("Another export is running (job " + std::to_string(current_->id) + ").");
  if (worker_.joinable()) worker_.join();  // the previous job has finished: its thread ends at once
  auto job = std::make_shared<Job>();
  job->id = nextId_++;
  job->request = std::move(request);
  job->frames = frames;
  job->skip = skip;
  current_ = job;
  jobs_[job->id] = job;
  while (jobs_.size() > 16) jobs_.erase(jobs_.begin());
  job->state = State::Running;
  worker_ = std::thread([this, job] { run(job); });
  return describe(*job);
}

void RenderJobs::run(std::shared_ptr<Job> job) {
  const RenderJobRequest& r = job->request;
  const size_t count = r.outputs.size();
  const uint64_t t0 = nowNs();
  // BUG-SWEEP-02: each working file is reserved with a unique name this job owns (exclusive create,
  // never an existing file). Cleanup removes only these: a retained "<target>.part" of an earlier
  // operation is never touched, and two processes exporting the same target never share a path.
  std::vector<std::string> working(count);
  std::vector<char> owned(count, 0);
  auto discard = [&] {
    std::error_code ignored;
    for (size_t f = 0; f < count; ++f)
      if (owned[f]) fs::remove(utf8(working[f]), ignored);
  };
  try {
    // Every song sample up to the region's end and its tail is played; the ones before the region are
    // dropped. Progress counts both, over all the files.
    const int64_t total = job->skip + job->frames;
    WorkerPool workers(WorkerPool::forExport());  // M4a: the tracks on several cores, the same file bit for bit
    job->threads = workers.threads();
    for (size_t f = 0; f < count && !job->cancel.load(); ++f) {
      job->file = (int)f;
      Graph graph(r.project, r.sampleRate, r.assets.get(), r.plugins, true);  // the job's own instruments and plugins; audio buffers are read-only
      graph.setSolo(r.outputs[f].track, r.outputs[f].withoutMasterGain);
      // Audio the song needs and cannot be read (moved, deleted, unreadable since the song was loaded): the job
      // fails before any file is written, so an export with holes never replaces the previous one (audit of main
      // fa3b428, F01).
      if (!graph.missingAssets().empty()) {
        std::string list;
        for (const auto& m : graph.missingAssets()) list += (list.empty() ? "" : "; ") + m;
        throw std::runtime_error("Missing audio, nothing exported: " + list);
      }
      // S1: a needed plugin that could not load refuses the export before it renders — a silent track
      // must never replace the previous export (bypassed units are intentional silence, not failures).
      checkExportPlugins(graph);
      graph.useWorkers(&workers);
      graph.preroll();  // effect latency: the song starts on sample 0 of the file
      working[f] = reserveRenderWorkingFile(r.outputs[f].path);
      owned[f] = 1;
      WavWriter wav(working[f], (int)r.sampleRate, r.bits, r.outputs[f].mono ? 1 : 2);
      std::vector<float> L(kBlock), R(kBlock);
      ExportLevel level;
      int n = 0;
      for (int64_t pos = 0; pos < total; pos += n) {
        if (job->cancel.load()) break;
        // Blocks end on the region's start, so the first written sample is the region's first.
        const int64_t until = pos < job->skip ? job->skip : total;
        n = (int)std::min<int64_t>(kBlock, until - pos);
        std::fill(L.begin(), L.begin() + n, 0.0f);
        std::fill(R.begin(), R.begin() + n, 0.0f);
        graph.process(L.data(), R.data(), n, pos, false);
        if (pos >= job->skip) {
          level.addFrames(L.data(), R.data(), n, r.outputs[f].mono);
          job->peak = std::max(job->peak, level.peak);
          wav.write(L.data(), R.data(), n);
        }
        job->progress = ((double)f + (double)(pos + n) / (double)std::max<int64_t>(1, total)) / (double)count;
      }
      // A cancelled file's writer closes when it goes out of scope; then this job's working files are removed.
      if (!job->cancel.load()) {
        // S1: a needed plugin that failed DURING the render (exception, crash, bad audio) refuses the whole
        // export here — before publish, so every target keeps its previous content (stems: the whole set).
        checkExportPlugins(graph);
        wav.finish(); checkExportLevel(level, r.outputs[f]);
      }
    }
    if (testHooks.beforePublish) testHooks.beforePublish(job->id);
    // One transition decides between cancel and publish: Running -> Publishing here, or Running ->
    // Cancelling in cancel(). Whichever happens first wins; the other side sees it.
    State running = State::Running;
    if (job->cancel.load() || !job->state.compare_exchange_strong(running, State::Publishing)) {
      discard();
      job->renderMs = (nowNs() - t0) / 1e6;
      job->state = State::Cancelled;
      return;
    }
    if (testHooks.afterPublishClaimed) testHooks.afterPublishClaimed(job->id);
    publishRenderOutputs(r.outputs, job->published, [&] { if (testHooks.beforePlace) testHooks.beforePlace(job->id); }, working, &job->rollback);
    job->renderMs = (nowNs() - t0) / 1e6;
    job->progress = 1;
    job->state = State::Succeeded;  // only now: the final files are closed and in place
  } catch (const std::exception& ex) {
    discard();
    job->error = ex.what();
    job->renderMs = (nowNs() - t0) / 1e6;
    job->state = State::Failed;
  }
}

json RenderJobs::describe(const Job& job) const {
  const State s = job.state.load();
  json out = {{"jobId", job.id}, {"state", name(s)}, {"progress", job.progress.load()}, {"path", job.request.path},
              {"revision", job.request.revision >= 0 ? json(job.request.revision) : json()},
              {"bits", job.request.bits}, {"sampleRate", job.request.sampleRate}, {"tailMs", job.request.tailSeconds * 1000},
              {"seconds", job.frames / job.request.sampleRate}, {"startTick", job.request.startTick},
              {"endTick", job.request.endTick}, {"file", job.file.load()}};
  json files = json::array();
  for (const auto& o : job.request.outputs)
    files.push_back({{"path", o.path}, {"track", o.track >= 0 ? json(job.request.project.tracks[(size_t)o.track].id) : json()}});
  out["files"] = files;
  if (finished(s)) {
    out["renderMs"] = job.renderMs;
    out["threads"] = job.threads + 1;
    if (s == State::Succeeded) out["peak"] = job.peak;
    if (s == State::Failed) {
      out["error"] = job.error;
      if (job.published > 0) out["published"] = job.published;  // new files the failure could not take back
      if (job.rollback.attempted) {
        out["rollbackAttempted"] = true;
        out["restored"] = job.rollback.restored;  // old files put back at their paths
        out["backups"] = job.rollback.backups;    // old copies kept as .previous: named so they can be recovered
      }
    }
  }
  return out;
}

json RenderJobs::status(uint64_t jobId) const {
  std::lock_guard<std::mutex> g(mutex_);
  auto it = jobs_.find(jobId);
  if (it == jobs_.end()) throw std::runtime_error("No export job " + std::to_string(jobId) + ".");
  return describe(*it->second);
}

json RenderJobs::cancel(uint64_t jobId) {
  std::lock_guard<std::mutex> g(mutex_);
  auto it = jobs_.find(jobId);
  if (it == jobs_.end()) throw std::runtime_error("No export job " + std::to_string(jobId) + ".");
  Job& job = *it->second;
  State running = State::Running;
  if (job.state.compare_exchange_strong(running, State::Cancelling)) {
    job.cancel = true;
    return describe(job);
  }
  json out = describe(job);
  // Too late: the file is being put in place; the job ends succeeded (or failed, if the rename fails).
  if (running == State::Publishing) out["cancelRefused"] = true;
  return out;
}

std::vector<json> RenderJobs::takeUpdates() {
  std::lock_guard<std::mutex> g(mutex_);
  std::vector<json> out;
  for (auto& [id, job] : jobs_) {
    State s = job->state.load();
    if (s == State::Publishing) s = State::Running;  // the same "running" for the app: not a change
    const double p = job->progress.load();
    if (s != job->reportedState || (!finished(s) && p - job->reportedProgress >= 0.01)) {
      job->reportedState = s;
      job->reportedProgress = p;
      out.push_back(describe(*job));
    }
  }
  return out;
}

} // namespace core
