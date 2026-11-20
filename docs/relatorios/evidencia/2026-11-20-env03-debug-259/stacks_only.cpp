// stacks_only.cpp — capture all threads of a running process (dbghelp, remote StackWalk64).
// usage: stacks_only.exe <pid> [label]
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#pragma comment(lib, "dbghelp.lib")

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI* NtQueryInformationThread_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);

static void walkThread(DWORD tid, HANDLE proc, NtQueryInformationThread_t ntQueryThread) {
  HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
  if (!th) { std::printf("  thread %lu: OpenThread failed (%lu)\n", tid, GetLastError()); return; }
  void* startAddr = nullptr;
  if (ntQueryThread) { ULONG ret = 0; ntQueryThread(th, 9, &startAddr, sizeof startAddr, &ret); }
  const DWORD susp = SuspendThread(th);
  if (susp == (DWORD)-1) { std::printf("  thread %lu: SuspendThread failed (%lu)\n", tid, GetLastError()); CloseHandle(th); return; }
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_FULL;
  if (!GetThreadContext(th, &ctx)) { std::printf("  thread %lu: GetThreadContext failed (%lu)\n", tid, GetLastError()); ResumeThread(th); CloseHandle(th); return; }
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

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: stacks_only <pid> [label]\n"); return 2; }
  const DWORD pid = (DWORD)atoi(argv[1]);
  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_SUSPEND_RESUME, FALSE, pid);
  if (!proc) { std::printf("OpenProcess(%lu) failed: %lu\n", pid, GetLastError()); return 2; }
  auto ntQueryThread = (NtQueryInformationThread_t)GetProcAddress(GetModuleHandleW(L"ntdll"), "NtQueryInformationThread");
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_AUTO_PUBLICS);
  const DWORD t0 = GetTickCount();
  BOOL ok = SymInitialize(proc, "srv*D:\\Core-build\\env03\\symcache*https://msdl.microsoft.com/download/symbols", TRUE);
  if (!ok) { std::printf("SymInitialize with path failed (%lu); local only\n", GetLastError()); ok = SymInitialize(proc, nullptr, TRUE); }
  std::printf("SymInitialize ok=%d (%lu ms), label=%s\n", ok, GetTickCount() - t0, argc > 2 ? argv[2] : "-");
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  int count = 0;
  if (Thread32First(snap, &te)) {
    do { if (te.th32OwnerProcessID == pid) { walkThread(te.th32ThreadID, proc, ntQueryThread); ++count; } } while (Thread32Next(snap, &te));
  }
  CloseHandle(snap);
  std::printf("=== %d threads ===\n", count);
  SymCleanup(proc);
  CloseHandle(proc);
  return 0;
}
