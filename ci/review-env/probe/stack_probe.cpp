// stack_probe.cpp - proves the Debug build's PDBs resolve real Core frames to file:line.
//
// What it does: a worker thread loops inside core::TempoMap::sampleAt / tickAt, which are compiled
// into the real build's core_engine_lib.lib (engine/src/core/tempo_map.cpp). The main thread suspends
// that worker, reads its CONTEXT and walks the stack with StackWalk64 + SymFromAddr +
// SymGetLineFromAddr64 (dbghelp). Expected output: frames resolving to
// engine\src\core\tempo_map.cpp:<line>, i.e. the debugger has usable symbols for Core code.
//
// Build (run by run-all.ps1 step 04):
//   cl /nologo /MTd /Zi /EHsc /utf-8 /W3 /I <src>\engine\src stack_probe.cpp /Fe:stack_probe.exe
//      /link /DEBUG <build>\core_engine_lib.lib dbghelp.lib
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "core/tempo_map.hpp"
#pragma comment(lib, "dbghelp.lib")

static void printStack(HANDLE thread, CONTEXT* ctx) {
  HANDLE proc = GetCurrentProcess();
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
  if (!SymInitialize(proc, nullptr, TRUE)) {
    std::printf("SymInitialize failed: %lu\n", GetLastError());
    return;
  }
  STACKFRAME64 sf{};
  sf.AddrPC.Mode = AddrModeFlat;    sf.AddrPC.Offset = ctx->Rip;
  sf.AddrFrame.Mode = AddrModeFlat; sf.AddrFrame.Offset = ctx->Rbp;
  sf.AddrStack.Mode = AddrModeFlat; sf.AddrStack.Offset = ctx->Rsp;
  CONTEXT c = *ctx;
  std::printf("--- stack frames (SymGetLineFromAddr64) ---\n");
  for (int n = 0; n < 24; ++n) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, thread, &sf, &c, nullptr,
                     SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    const DWORD64 pc = sf.AddrPC.Offset;
    if (!pc) break;
    char symBuf[sizeof(SYMBOL_INFO) + 256] = {};
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    DWORD64 disp = 0;
    char name[96] = "?";
    if (SymFromAddr(proc, pc, &disp, sym)) strncpy_s(name, sizeof(name), sym->Name, _TRUNCATE);
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD dispLine = 0;
    char where[320] = "no source line";
    if (SymGetLineFromAddr64(proc, pc, &dispLine, &line))
      sprintf_s(where, sizeof(where), "%s:%lu (+%lu)", line.FileName, line.LineNumber, dispLine);
    std::printf("#%-2d 0x%016llx  %-42s  %s\n", n, static_cast<unsigned long long>(pc), name, where);
  }
  SymCleanup(proc);
}

static core::TempoMap* g_map = nullptr;
static volatile LONG g_spin = 1;

static DWORD WINAPI worker(LPVOID) {
  volatile double acc = 0;
  while (g_spin) {
    acc += g_map->sampleAt(123.45);   // Core frame: engine/src/core/tempo_map.cpp
    acc += g_map->tickAt(1000.0);     // Core frame: engine/src/core/tempo_map.cpp
  }
  (void)acc;
  return 0;
}

int main() {
  std::vector<core::TempoChange> changes{{960, 174.0, true}};
  core::TempoMap map(145.0, changes, 48000.0);
  g_map = &map;
  HANDLE th = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
  if (!th) { std::printf("CreateThread failed: %lu\n", GetLastError()); return 2; }
  Sleep(200);  // let the worker get deep into the Core call loop
  for (int attempt = 1; attempt <= 5; ++attempt) {
    SuspendThread(th);
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(th, &ctx)) {
      std::printf("=== attempt %d: sampled CONTEXT of the worker thread ===\n", attempt);
      printStack(th, &ctx);
    } else {
      std::printf("GetThreadContext failed: %lu\n", GetLastError());
    }
    ResumeThread(th);
    Sleep(40);
  }
  g_spin = 0;
  WaitForSingleObject(th, 2000);
  CloseHandle(th);
  std::printf("probe done\n");
  return 0;
}
