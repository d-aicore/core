// env03_probe.cpp — ENV-03: measures the REAL shutdown timeline of core-engine serve from outside.
// INVESTIGATION ONLY: drives the crash/retry scenario of engine_process_tests.cpp:96-160 over raw pipes
// (same pipe/handle-list setup as app/engine-client), timestamps stdin close, stdout EOF and process exit,
// and captures the stacks of ALL engine threads (StackWalk64 + SymFromAddr + SymGetLineFromAddr64, dbghelp)
// while the engine is still alive past the client's 2 s budget.
//
// usage: env03_probe.exe <engine.exe> <crash.vst3> <synth.vst3> <label> [maxWaitMs=30000]
// writes: D:\Core-build\env03\logs\<label>-stderr.log (engine stderr, timestamped chunks)
// prints: per-command timeline + shutdown timeline + thread stacks (redirect to <label>-probe.log)
//
// build: cl /nologo /MTd /Zi /EHsc /std:c++20 /utf-8 /W3 /I D:\CORE\CORE\engine\third_party
//        env03_probe.cpp /link /DEBUG dbghelp.lib
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <share.h>
#include <crtdbg.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "json.hpp"
#pragma comment(lib, "dbghelp.lib")

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
static Clock::time_point g_t0;  // probe start
static double nowMs() { return std::chrono::duration<double, std::milli>(Clock::now() - g_t0).count(); }
static void say(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  std::printf("[%10.1f ms] ", nowMs());
  std::vprintf(fmt, ap);
  std::printf("\n"); std::fflush(stdout);
  va_end(ap);
}

// ---------------------------------------------------------------- child process + pipes (as EngineClient::start)
struct Child {
  HANDLE process = nullptr, job = nullptr, thread = nullptr;
  HANDLE stdinWrite = nullptr, stdoutRead = nullptr, stderrRead = nullptr;
  DWORD pid = 0;
};
static bool spawn(const std::wstring& exe, const std::wstring& args, Child& c) {
  SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
  HANDLE inR = nullptr, inW = nullptr, outR = nullptr, outW = nullptr, errR = nullptr, errW = nullptr;
  auto closeAll = [&] { for (HANDLE h : {inR, inW, outR, outW, errR, errW}) if (h) CloseHandle(h); };
  if (!CreatePipe(&inR, &inW, &sa, 0) || !CreatePipe(&outR, &outW, &sa, 0) || !CreatePipe(&errR, &errW, &sa, 0)) { closeAll(); return false; }
  for (HANDLE h : {inW, outR, errR}) SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0);
  HANDLE inherit[] = {inR, outW, errW};
  SIZE_T size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
  std::vector<char> attributes(size);
  auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
  if (!InitializeProcThreadAttributeList(list, 1, 0, &size) ||
      !UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit, nullptr, nullptr)) { closeAll(); return false; }
  STARTUPINFOEXW si{};
  si.StartupInfo.cb = sizeof si;
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = inR; si.StartupInfo.hStdOutput = outW; si.StartupInfo.hStdError = errW;
  si.lpAttributeList = list;
  std::wstring cmd = L"\"" + exe + L"\" " + args;
  PROCESS_INFORMATION pi{};
  const BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi);
  DeleteProcThreadAttributeList(list);
  if (!ok) { say("CreateProcess failed: %lu", GetLastError()); closeAll(); return false; }
  c.process = pi.hProcess; c.thread = pi.hThread; c.pid = pi.dwProcessId;
  c.job = CreateJobObjectW(nullptr, nullptr);  // cleanup only: the engine dies if this probe dies
  if (c.job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(c.job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    AssignProcessToJobObject(c.job, pi.hProcess);
  }
  CloseHandle(inR); CloseHandle(outW); CloseHandle(errW);
  c.stdinWrite = inW; c.stdoutRead = outR; c.stderrRead = errR;
  return true;
}

// ---------------------------------------------------------------- protocol reader (timestamps EOF and last line)
struct Reader {
  std::mutex m;
  std::condition_variable cv;
  std::map<int64_t, json> replies;
  std::atomic<bool> ready{false}, eof{false};
  std::atomic<double> eofAt{-1}, lastLineAt{-1}, firstLineAt{-1};
  std::atomic<long> lines{0}, events{0}, badLines{0};
  HANDLE out = nullptr;
  std::string logPath;
  FILE* lineLog = nullptr;  // every stdout line, timestamped
  void run() {
    fopen_s(&lineLog, logPath.c_str(), "wb");
    std::string partial;
    std::vector<char> buf(1 << 16);
    DWORD n = 0;
    while (ReadFile(out, buf.data(), (DWORD)buf.size(), &n, nullptr) && n > 0) {
      partial.append(buf.data(), n);
      size_t start = 0, end;
      while ((end = partial.find('\n', start)) != std::string::npos) {
        std::string line = partial.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const double t = nowMs();
        lastLineAt.store(t);
        if (firstLineAt.load() < 0) firstLineAt.store(t);
        lines.fetch_add(1);
        if (lineLog) { std::fprintf(lineLog, "[%10.1f ms] %s\n", t, line.c_str()); std::fflush(lineLog); }
        // Nothing the engine writes may end this thread (as the real client): a bad line is logged, never fatal.
        try {
          json j = json::parse(line, nullptr, false);
          if (j.is_discarded() || !j.is_object()) continue;
          if (j.contains("event") && !j.contains("id")) {
            events.fetch_add(1);
            if (j.value("event", "") == "ready") { std::lock_guard<std::mutex> g(m); ready.store(true); }
            cv.notify_all();
            continue;
          }
          if (j.contains("id") && j["id"].is_number_integer()) {
            const int64_t id = j["id"].get<int64_t>();  // key FIRST, then move (MSVC eval-order pitfall)
            std::lock_guard<std::mutex> g(m);
            replies[id] = std::move(j);
            cv.notify_all();
          }
        } catch (const std::exception& e) {
          badLines.fetch_add(1);
          say("BAD LINE (%s): %.200s", e.what(), line.c_str());
        } catch (...) {
          badLines.fetch_add(1);
          say("BAD LINE (unknown): %.200s", line.c_str());
        }
      }
      partial.erase(0, start);
    }
    eofAt.store(nowMs());
    eof.store(true);
    say("STDOUT EOF (ReadFile ended, GetLastError=%lu)", GetLastError());
    if (lineLog) { std::fprintf(lineLog, "[%10.1f ms] === STDOUT EOF ===\n", nowMs()); std::fclose(lineLog); lineLog = nullptr; }
    cv.notify_all();
  }
};

static Child g_child;
static Reader g_reader;
static int64_t g_nextId = 1;
static void captureAllStacks(HANDLE proc, DWORD pid, const char* when);

static json command(const std::string& cmd, const json& args, double timeoutMs) {
  const int64_t id = g_nextId++;
  const std::string line = json{{"id", id}, {"cmd", cmd}, {"args", args}}.dump() + "\n";
  const double t = nowMs();
  DWORD written = 0;
  if (!WriteFile(g_child.stdinWrite, line.data(), (DWORD)line.size(), &written, nullptr) || written != line.size())
    throw std::runtime_error("stdin write failed for " + cmd);
  std::unique_lock<std::mutex> lock(g_reader.m);
  const bool got = g_reader.cv.wait_for(lock, std::chrono::milliseconds((int64_t)timeoutMs), [&] { return g_reader.replies.count(id) != 0; });
  if (!got) {
    lock.unlock();
    say("CMD %s -> TIMEOUT (%.0f ms); capturing engine stacks", cmd.c_str(), timeoutMs);
    captureAllStacks(g_child.process, g_child.pid, "command timeout");
    throw std::runtime_error("timeout: " + cmd);
  }
  json r = std::move(g_reader.replies[id]);
  g_reader.replies.erase(id);
  lock.unlock();
  const double dt = nowMs() - t;
  const bool ok = r.value("ok", false);
  say("CMD %-16s -> %s (%.1f ms)%s", cmd.c_str(), ok ? "ok" : "ERR", dt, ok ? "" : (" " + r.value("error", std::string())).c_str());
  if (!ok) throw std::runtime_error(cmd + ": " + r.value("error", std::string()));
  return r.contains("result") ? r["result"] : json::object();
}

// ---------------------------------------------------------------- stderr capture (timestamped chunks)
static void stderrLoop(const std::string& path) {
  FILE* f = _fsopen(path.c_str(), "wb", _SH_DENYNO);  // shared: readable while the probe runs
  char buf[4096];
  DWORD n = 0;
  while (ReadFile(g_child.stderrRead, buf, sizeof buf, &n, nullptr) && n > 0) {
    if (f) { std::fprintf(f, "[%10.1f ms] ", nowMs()); std::fwrite(buf, 1, n, f); std::fflush(f); }
  }
  if (f) { std::fprintf(f, "[%10.1f ms] STDERR EOF\n", nowMs()); std::fclose(f); }
}

// ---------------------------------------------------------------- remote thread stacks (dbghelp)
typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI* NtQueryInformationThread_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
static NtQueryInformationThread_t ntQueryThread = nullptr;

static void walkThread(DWORD tid, HANDLE proc) {
  HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
  if (!th) { say("  thread %lu: OpenThread failed (%lu)", tid, GetLastError()); return; }
  void* startAddr = nullptr;
  if (ntQueryThread) { ULONG ret = 0; ntQueryThread(th, 9 /*ThreadQuerySetWin32StartAddress*/, &startAddr, sizeof startAddr, &ret); }
  const DWORD susp = SuspendThread(th);
  if (susp == (DWORD)-1) { say("  thread %lu: SuspendThread failed (%lu)", tid, GetLastError()); CloseHandle(th); return; }
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_FULL;
  if (!GetThreadContext(th, &ctx)) { say("  thread %lu: GetThreadContext failed (%lu)", tid, GetLastError()); ResumeThread(th); CloseHandle(th); return; }
  char startSym[160] = "?";
  if (startAddr) {
    char sb[sizeof(SYMBOL_INFO) + 256] = {};
    auto* si = reinterpret_cast<SYMBOL_INFO*>(sb);
    si->SizeOfStruct = sizeof(SYMBOL_INFO); si->MaxNameLen = 255;
    DWORD64 d = 0;
    if (SymFromAddr(proc, (DWORD64)startAddr, &d, si)) sprintf_s(startSym, "%s+0x%llx", si->Name, (unsigned long long)d);
    else sprintf_s(startSym, "0x%llx", (unsigned long long)startAddr);
  }
  std::printf("  --- thread %lu (start=%s, rip=0x%llx) ---\n", tid, startSym, (unsigned long long)ctx.Rip);
  STACKFRAME64 sf{};
  sf.AddrPC.Mode = AddrModeFlat;    sf.AddrPC.Offset = ctx.Rip;
  sf.AddrFrame.Mode = AddrModeFlat; sf.AddrFrame.Offset = ctx.Rbp;
  sf.AddrStack.Mode = AddrModeFlat; sf.AddrStack.Offset = ctx.Rsp;
  CONTEXT c = ctx;
  for (int i = 0; i < 40; ++i) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th, &sf, &c, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    const DWORD64 pc = sf.AddrPC.Offset;
    if (!pc) break;
    char sb[sizeof(SYMBOL_INFO) + 256] = {};
    auto* si = reinterpret_cast<SYMBOL_INFO*>(sb);
    si->SizeOfStruct = sizeof(SYMBOL_INFO); si->MaxNameLen = 255;
    DWORD64 disp = 0;
    char name[128] = "?";
    if (SymFromAddr(proc, pc, &disp, si)) strncpy_s(name, si->Name, _TRUNCATE);
    IMAGEHLP_MODULE64 mi{};
    mi.SizeOfStruct = sizeof mi;
    char mod[64] = "?";
    if (SymGetModuleInfo64(proc, pc, &mi)) strncpy_s(mod, mi.ModuleName, _TRUNCATE);
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof line;
    DWORD dl = 0;
    char where[320] = "";
    if (SymGetLineFromAddr64(proc, pc, &dl, &line)) sprintf_s(where, "  %s:%lu", line.FileName, line.LineNumber);
    std::printf("    #%-2d %s!%-52s +0x%-6llx%s\n", i, mod, name, (unsigned long long)disp, where);
  }
  std::fflush(stdout);
  ResumeThread(th);
  CloseHandle(th);
}

static void captureAllStacks(HANDLE proc, DWORD pid, const char* when) {
  say("=== STACK CAPTURE (%s) ===", when);
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) { say("snapshot failed (%lu)", GetLastError()); return; }
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  int count = 0;
  if (Thread32First(snap, &te)) {
    do {
      if (te.th32OwnerProcessID == pid) { walkThread(te.th32ThreadID, proc); ++count; }
    } while (Thread32Next(snap, &te));
  }
  CloseHandle(snap);
  say("=== END STACK CAPTURE (%d threads) ===", count);
}

// Compact phase sample: top 4 frames of every live thread, one line each (100 ms cadence after stdin close).
static void samplePhases(HANDLE proc, DWORD pid) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return;
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  if (Thread32First(snap, &te)) {
    do {
      if (te.th32OwnerProcessID != pid) continue;
      HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
      if (!th) continue;
      if (SuspendThread(th) == (DWORD)-1) { CloseHandle(th); continue; }
      CONTEXT ctx{};
      ctx.ContextFlags = CONTEXT_FULL;
      if (GetThreadContext(th, &ctx)) {
        STACKFRAME64 sf{};
        sf.AddrPC.Mode = AddrModeFlat;    sf.AddrPC.Offset = ctx.Rip;
        sf.AddrFrame.Mode = AddrModeFlat; sf.AddrFrame.Offset = ctx.Rbp;
        sf.AddrStack.Mode = AddrModeFlat; sf.AddrStack.Offset = ctx.Rsp;
        CONTEXT c = ctx;
        char line[512] = "";
        size_t off = 0;
        for (int i = 0; i < 4; ++i) {
          if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th, &sf, &c, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
          const DWORD64 pc = sf.AddrPC.Offset;
          if (!pc) break;
          char sb[sizeof(SYMBOL_INFO) + 256] = {};
          auto* si = reinterpret_cast<SYMBOL_INFO*>(sb);
          si->SizeOfStruct = sizeof(SYMBOL_INFO); si->MaxNameLen = 255;
          DWORD64 d = 0;
          char name[128] = "?";
          if (SymFromAddr(proc, pc, &d, si)) strncpy_s(name, si->Name, _TRUNCATE);
          IMAGEHLP_MODULE64 mi{};
          mi.SizeOfStruct = sizeof mi;
          char mod[48] = "?";
          if (SymGetModuleInfo64(proc, pc, &mi)) strncpy_s(mod, mi.ModuleName, _TRUNCATE);
          off += (size_t)sprintf_s(line + off, sizeof line - off, "%s%s!%s", i ? " <- " : "", mod, name);
          if (off > sizeof line - 160) break;
        }
        say("PHASE tid=%lu %s", te.th32ThreadID, line);
      }
      ResumeThread(th);
      CloseHandle(th);
    } while (Thread32Next(snap, &te));
  }
  CloseHandle(snap);
}

// ---------------------------------------------------------------- the scenario (engine_process_tests.cpp:96-160)
static json scanFirst(const std::wstring& exe, const std::string& file) {
  std::string narrow(exe.begin(), exe.end());
  const std::string cmd = "\"\"" + narrow + "\" scan \"" + file + "\"\"";
  FILE* pipe = _popen(cmd.c_str(), "r");
  if (!pipe) return json();
  std::string out;
  char buf[4096];
  while (size_t n = std::fread(buf, 1, sizeof buf, pipe)) out.append(buf, n);
  _pclose(pipe);
  const json found = json::parse(out, nullptr, false);
  return found.is_array() && !found.empty() ? found[0] : json();
}
static json pluginTrack(const std::string& id, const json& plugin) {
  return {{"id", id}, {"name", id}, {"instrument", "plugin"}, {"plugin", plugin}, {"patternTicks", 3840},
          {"notes", json::array({{{"tick", 0}, {"length", 960}, {"pitch", 60}, {"velocity", 0.8}}})}};
}
static json song(const json& tracks) {
  return {{"format", "core-project"}, {"version", 2}, {"name", "plugin crash"}, {"bpm", 120.0},
          {"lengthTicks", 64 * 960}, {"masterGainDb", 0.0}, {"tracks", tracks}};
}

int wmain(int argc, wchar_t** argv) {
  g_t0 = Clock::now();
  // No modal dialogs from the Debug CRT (a frozen MessageBox would hide the real measurement).
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  if (argc < 5) { std::printf("usage: env03_probe <engine.exe> <crash.vst3> <synth.vst3> <label> [maxWaitMs]\n"); return 2; }
  const std::wstring exe = argv[1];
  const double maxWaitMs = argc > 5 ? _wtoi(argv[5]) : 30000;
  auto narrow = [](const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, 0);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
  };
  const std::string variant = argc > 6 ? narrow(argv[6]) : "full";   // full = the test scenario; single = one crash, no retry
  const bool phases = argc > 7 && narrow(argv[7]) == "phases";       // 100 ms stack sampling after stdin close
  const std::string crashPath = narrow(argv[2]), synthPath = narrow(argv[3]), lab = narrow(argv[4]);
  say("probe label=%s variant=%s phases=%d engine=%ls", lab.c_str(), variant.c_str(), phases ? 1 : 0, exe.c_str());
  say("crash plugin: %s", crashPath.c_str());
  say("synth plugin: %s", synthPath.c_str());

  const json crash = scanFirst(exe, crashPath), synth = scanFirst(exe, synthPath);
  if (!crash.is_object() || !synth.is_object()) { say("SCAN FAILED (crash=%s synth=%s)", crash.dump().c_str(), synth.dump().c_str()); return 2; }
  say("scan ok: crash='%s' synth='%s'", crash.value("name", "").c_str(), synth.value("name", "").c_str());

  if (!spawn(exe, L"serve", g_child)) return 2;
  say("engine started pid=%lu", g_child.pid);
  g_reader.out = g_child.stdoutRead;
  g_reader.logPath = "D:\\Core-build\\env03\\logs\\" + lab + "-stdout.log";
  std::thread reader([&] { g_reader.run(); });
  std::thread errors([&] { stderrLoop("D:\\Core-build\\env03\\logs\\" + lab + "-stderr.log"); });
  {
    std::unique_lock<std::mutex> lock(g_reader.m);
    if (!g_reader.cv.wait_for(lock, std::chrono::milliseconds(20000), [&] { return g_reader.ready.load() || g_reader.eof.load(); })) {
      say("no hello within 20 s"); TerminateProcess(g_child.process, 1); return 2;
    }
  }
  if (!g_reader.ready.load()) { say("engine exited before hello"); return 2; }
  say("hello received");

  int rc = 0;
  double stdinCloseAt = -1, exitAt = -1;
  DWORD exitCode = 259;
  bool forced = false, stacks1 = false, stacks2 = false, capA = false, capB = false, capC = false;
  try {
    const json firstSong = variant == "clean" ? song(json::array({pluginTrack("synth", synth)}))
                                              : song(json::array({pluginTrack("crash", crash), pluginTrack("synth", synth)}));
    const json loaded = command("project.load", {{"project", firstSong}}, 30000);
    if (!loaded.value("missing", json::array()).empty()) { say("UNEXPECTED missing plugins: %s", loaded.value("missing", json::array()).dump().c_str()); }
    command("engine.start", {{"backend", "null"}}, 10000);
    command("transport.play", json::object(), 10000);
    json failed = json::array();
    if (variant != "clean") {
      for (int i = 0; i < 100 && failed.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        failed = command("plugins.failed", json::object(), 5000);
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));  // let the healthy synth play a while
    }
    say("plugins.failed size=%zu %s", failed.size(), failed.dump().c_str());
    if (variant == "single") {
      // One crashed (leaked) instance, no retry, no track removal: measures the teardown with HALF the leaks.
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      command("transport.stop", json::object(), 2000);
      command("engine.stats", json::object(), 2000);
    } else if (variant == "clean") {
      // Control: no crash at all (the failed list stays empty): no leaked instances, teardown must be quick.
      command("transport.stop", json::object(), 2000);
      command("engine.stats", json::object(), 2000);
    } else {
    const json retry = command("plugin.retry", {{"key", "track:crash"}}, 10000);
    say("retry: %s", retry.dump().c_str());
    for (int i = 0; i < 10; ++i) {
      command("plugins.states", json::object(), 2000);
      command("engine.stats", json::object(), 2000);
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    command("project.load", {{"project", song(json::array({pluginTrack("synth", synth)}))}}, 10000);
    for (int i = 0; i < 10; ++i) {
      command("plugins.states", json::object(), 2000);
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    command("transport.stop", json::object(), 2000);
    command("engine.stats", json::object(), 2000);
    }
    say("scenario complete; preloading symbols for the target process (before stdin close)");
    const double symT = nowMs();
    ntQueryThread = (NtQueryInformationThread_t)GetProcAddress(GetModuleHandleW(L"ntdll"), "NtQueryInformationThread");
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_AUTO_PUBLICS);
    // PDBs of the engine/plugins are found via their embedded absolute paths; system DLLs via the symbol server
    // (falls back to export names when the download is not possible).
    BOOL symOk = SymInitialize(g_child.process, "srv*D:\\Core-build\\env03\\symcache*https://msdl.microsoft.com/download/symbols", TRUE);
    if (!symOk) { say("SymInitialize with symbol path failed (%lu); retrying local only", GetLastError()); symOk = SymInitialize(g_child.process, nullptr, TRUE); }
    say("SymInitialize ok=%d took %.0f ms", symOk, nowMs() - symT);

    // ---- the measurement: close stdin, then watch EOF and the real exit ----
    stdinCloseAt = nowMs();
    CloseHandle(g_child.stdinWrite); g_child.stdinWrite = nullptr;
    say("STDIN CLOSED (writer end closed; engine should end its session)");
    const double deadline = stdinCloseAt + maxWaitMs;
    double nextSampleAt = phases ? stdinCloseAt : 1e18;
    for (;;) {
      const DWORD w = WaitForSingleObject(g_child.process, 50);
      const double t = nowMs();
      if (w == WAIT_OBJECT_0) { exitAt = t; GetExitCodeProcess(g_child.process, &exitCode); break; }
      if (phases && t >= nextSampleAt) { nextSampleAt = t + (t < stdinCloseAt + 800 ? 10 : 200); samplePhases(g_child.process, g_child.pid); }
      if (phases && g_reader.eofAt.load() >= 0) {
        const double eof = g_reader.eofAt.load();
        if (!capA && t >= eof + 400)  { captureAllStacks(g_child.process, g_child.pid, "eof+0.4 s"); capA = true; }
        if (!capB && t >= eof + 1500) { captureAllStacks(g_child.process, g_child.pid, "eof+1.5 s"); capB = true; }
        if (!capC && t >= eof + 2600) { captureAllStacks(g_child.process, g_child.pid, "eof+2.6 s"); capC = true; }
      }
      const double eof = g_reader.eofAt.load();
      const double anchor = eof >= 0 ? eof : stdinCloseAt;
      const char* basis = eof >= 0 ? "eof" : "stdin-close";
      if (!stacks1 && t >= anchor + 2200) {
        char when[96]; sprintf_s(when, "%s+2.2 s, process STILL ACTIVE (the client would report 259 now)", basis);
        captureAllStacks(g_child.process, g_child.pid, when);
        stacks1 = true;
      }
      if (!stacks2 && t >= anchor + 6000) {
        char when[96]; sprintf_s(when, "%s+6 s, process STILL ACTIVE", basis);
        captureAllStacks(g_child.process, g_child.pid, when);
        stacks2 = true;
      }
      if (t >= deadline) {
        say("MAX WAIT %.0f ms reached: engine STILL ACTIVE (exit code would be 259); capturing then terminating", maxWaitMs);
        captureAllStacks(g_child.process, g_child.pid, "max wait");
        TerminateProcess(g_child.process, 1);
        WaitForSingleObject(g_child.process, 5000);
        exitAt = nowMs(); GetExitCodeProcess(g_child.process, &exitCode);
        forced = true;
        break;
      }
    }
  } catch (const std::exception& e) {
    say("SCENARIO EXCEPTION: %s", e.what());
    rc = 2;
    if (g_child.stdinWrite) { CloseHandle(g_child.stdinWrite); g_child.stdinWrite = nullptr; }
    WaitForSingleObject(g_child.process, 10000);
    exitAt = nowMs(); GetExitCodeProcess(g_child.process, &exitCode);
  }
  const double eof = g_reader.eofAt.load();
  say("TIMELINE stdinClose=%.1f ms stdoutEOF=%.1f ms (close->eof %.1f ms) processExit=%.1f ms (close->exit %.1f ms, eof->exit %.1f ms) exitCode=%lu forced=%d",
      stdinCloseAt, eof, eof >= 0 ? eof - stdinCloseAt : -1.0, exitAt, exitAt - stdinCloseAt, eof >= 0 ? exitAt - eof : -1.0, exitCode, forced ? 1 : 0);
  say("RESULT %s", json{{"label", lab}, {"variant", variant}, {"pid", g_child.pid}, {"stdinCloseMs", stdinCloseAt}, {"stdoutEofMs", eof},
                        {"exitMs", exitAt}, {"closeToEofMs", eof >= 0 ? eof - stdinCloseAt : -1.0},
                        {"closeToExitMs", exitAt - stdinCloseAt}, {"eofToExitMs", eof >= 0 ? exitAt - eof : -1.0},
                        {"exitCode", exitCode}, {"forced", forced}, {"stdoutLines", g_reader.lines.load()},
                        {"badLines", g_reader.badLines.load()}, {"lastStdoutLineMs", g_reader.lastLineAt.load()}}.dump().c_str());
  if (g_child.stdinWrite) CloseHandle(g_child.stdinWrite);
  reader.join();
  errors.join();
  SymCleanup(g_child.process);
  CloseHandle(g_child.thread);
  CloseHandle(g_child.process);
  CloseHandle(g_child.job);
  return rc;
}
