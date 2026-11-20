# ENV-03 investigation notes (env03-debug, Stage 3A)

Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 + uncommitted round-1 fixes (verified via git rev-parse/status).
Scope: INVESTIGATION ONLY. Writes confined to D:\Core-build\env03\ and this file. No repo edits.
Builds used (binaries only, no rebuild): Debug = D:\Core-build\s1-fix\build, Release = D:\Core-build\gate-round1.

## Step 0 — code reading (client + engine serve), 2026-11-20 ~01:32 WET

Client close path (app/engine-client/src/engine_client.cpp, read in full):
- close() (:160-179): sets stopping_, notifies; writer thread flushes outbox then CloseHandle(stdinWrite) (:252).
  close() itself waits WaitForSingleObject(process, closeTimeout=10000ms in tests) and TerminateProcess(1) on timeout (:167-168).
- readLoop (:271-339): ReadFile(stdout) until EOF (all write ends of the stdout pipe closed). THEN :333
  WaitForSingleObject(process, 2000); :335 GetExitCodeProcess -> 259 (STILL_ACTIVE) if not exited within 2 s of EOF;
  :336-338 disconnect("The engine exited (code N).") -> event handler -> EndReason -> test CHECK at engine_process_tests.cpp:159.
- KEY INFERENCE: «code 259» means the stdout pipe reached EOF while the engine process was STILL RUNNING >2 s later.
  If EOF were caused by process termination itself, the wait would succeed and the real code would be read.
  So either (a) the engine/CRT closes the protocol stdout handle before teardown finishes, or (b) another holder of the
  pipe write end closed it early. Must measure which.

Engine serve path (engine/src/app/main.cpp, read in full):
- keepStdoutForProtocol() (:113-122): _dup(stdout fd) -> g_protocol FILE*; then _dup2(stderr fd, stdout fd) — the ORIGINAL
  stdout pipe handle is closed at STARTUP and survives only inside g_protocol. Protocol writes go to g_protocol (:123-127).
  => stdout-pipe EOF happens when g_protocol's underlying handle closes (fclose/CRT exit) OR at process termination.
- serve() (:131-177): Server on the protocol thread's stack; ticker thread 40 ms; stdin read loop; on stdin EOF:
  running=false; ticker.join(); return 0 -> Server destructor (stack unwind) -> back to runWithMessageLoop.
- main (:583-587): core::plugins::Runtime juce; EnginePlugins plugins; runWithMessageLoop([&]{ return serve(&plugins); }).
  Main thread = JUCE message thread; serve runs on its own thread. After serve returns: message loop ends, then
  EnginePlugins/JuceHost and Runtime destructors (JUCE shutdown) — order to confirm in juce_host.
- Test harness (tests/support/check.hpp): argv[1] substring filter; exit 0 green; failures print "FALHOU file:line" to stderr.

Open questions:
1. When exactly does stdout EOF occur relative to real process exit (Debug vs Release)? -> probe (task 2).
2. What is the engine doing between EOF and exit (or is it a genuine hang)? -> thread stacks (task 3).
3. Where does g_protocol get closed before exit, if it does? CRT exit path vs explicit fclose vs JUCE teardown.

## Step 1 — reproduction runs (2026-11-20 ~01:36 WET)

Build types verified from CMakeCache.txt: s1-fix\build = Debug, gate-round1 = Release.
Filter = substring of test name; logs in D:\Core-build\env03\logs\.

| config | case | run | test exit | wall | result |
|---|---|---|---|---|---|
| Debug s1-fix | crashes in processBlock | 1 | 1 | 6.0 s | FAIL :159 «code 259» |
| Debug s1-fix | crashes in processBlock | 2 | 1 | 6.0 s | FAIL :159 «code 259» |
| Debug s1-fix | crashes in processBlock | 3 | 1 | 6.0 s | FAIL :159 «code 259» |
| Release gate-round1 | crashes in processBlock | 1 | 0 | 3.4 s | PASS (25 checks) |
| Release gate-round1 | crashes in processBlock | 2 | 0 | 3.4 s | PASS |
| Release gate-round1 | crashes in processBlock | 3 | 0 | 3.4 s | PASS |
| Debug s1-fix | restart the component | 1 | 0 | 1.8 s | PASS (17 checks; also ends with close + code-0 check) |
| Debug s1-fix | bounded reset | 1 | 0 | 8.5 s | PASS (10 checks) |
| Debug s1-fix | throws while playing | 1 | 0 | 0.8 s | PASS (10 checks) |

Exact failure (debug-crash-run1.log): FALHOU engine_process_tests.cpp:159: end.get() == "The engine exited (code 0).":
medido «The engine exited (code 259).» — 1 failure / 25 checks; all other checks green (slowest answer after crash: 1 ms).

Scoping: 259 is NOT general Debug shutdown — three other cases that also close stdin and check code 0 pass in the same
Debug build. Specific to the crash/retry case. Differences of the failing case vs the passing ones:
- TWO leaked crashed instances (original + retried one that crashes again 0.1 s into playback), vs one in restart case.
- A second project.load that REMOVES the crash track (graph rebuild) before stop.
- transport.stop BEFORE close (restart case closes while still playing; throw case also stops first but has only ONE
  leaked instance and no track removal).
- Whole Debug run ~6 s: scenario ~3.7 s + ~2.3 s in close. Since close() would wait up to 10 s before terminating,
  the ~6 s wall suggests the engine DID exit by itself ~2.3 s after stdin close — just past the reader's 2 s budget.
  HYPOTHESIS (not proven): slow-but-finite Debug teardown, stdout EOF early in teardown. Probe must measure.
  [CONFIRMED by step 2 — and the cause is the Debug CRT leak dump of the leaked instances, see steps 3-5.]

## Step 2 — external shutdown-timeline probe (2026-11-20 ~01:41–02:05 WET)

Probe: D:\Core-build\env03\probe\env03_probe.cpp (+ build-probe.bat, stacks_only.cpp). Raw pipes with the SAME
setup as EngineClient::start (CreatePipe, PROC_THREAD_ATTRIBUTE_HANDLE_LIST {inR,outW,errW}, CREATE_NO_WINDOW,
job KILL_ON_JOB_CLOSE for cleanup only). Drives the exact :96-160 scenario (scan both plugins via core-engine scan,
project.load crash+synth, engine.start null, transport.play, poll plugins.failed, plugin.retry, 10x states+stats,
project.load synth-only, 10x states, transport.stop, engine.stats), then timestamps: stdin close, stdout EOF
(reader thread), and the REAL process exit (WaitForSingleObject 50 ms poll, up to 30 s). Captures ALL engine thread
stacks remotely (OpenThread+SuspendThread+GetThreadContext+StackWalk64+SymFromAddr+SymGetLineFromAddr64, dbghelp,
build PDBs resolve Core/JUCE/plugin frames to file:line; system DLLs via exports/symbol server). Variants:
full (the test), single (one crash, no retry → ONE leaked instance), clean (synth only, NO crash → zero leaks).
Logs: D:\Core-build\env03\logs\<label>.log + <label>-stderr.log + <label>-stdout.log (every protocol line, timestamped).

Probe bug found and fixed on the first run (NOT an engine issue): reader threw on
`replies[j["id"].get<int64_t>()] = std::move(j)` (MSVC eval-order: j moved-from before the key read; Debug CRT then
froze the probe in an abort MessageBox). Fixed: key first, per-line try/catch (as the real client), CRT dialogs
suppressed, stderr log shared. The stuck engine's own stacks were captured first (stuck-39060-stacks.log): protocol
thread waiting on stdin, ticker blocked writing a FULL stdout pipe because the dead probe reader stopped draining —
engine healthy. First backgrounded job pwsh-b10bc0bd74a348ec killed; lead notified.

MEASURED TIMELINE (all runs: exitCode=0, forced=0 — the engine ALWAYS exits by itself; ms relative to stdin close):

| run | config | scenario | close→EOF | close→exit | eof→exit |
|---|---|---|---|---|---|
| debug-probe-1 | Debug s1-fix | full (2 leaks) | 48.9 | 2981.8 | 2932.9 |
| debug-probe-2 | Debug s1-fix | full | 36.0 | 3285.7 | 3249.7 |
| debug-probe-3 | Debug s1-fix | full | 67.9 | 2999.8 | 2931.9 |
| debug-probe-4 (old binary, full) | Debug s1-fix | full | 62.4 | 2722.9 | 2660.5 |
| debug-phases-1 | Debug s1-fix | full | 51.9 | 3245.3 | 3193.4 |
| debug-deep-1 | Debug s1-fix | full | 35.7 | 3099.5 | 3063.8 |
| debug-single-1 | Debug s1-fix | single (1 leak) | 50.6 | 1243.2 | 1192.6 |
| debug-clean-1 | Debug s1-fix | clean (0 leaks) | 57.5 | 177.7 | 120.2 |
| release-probe-1 | Release gate-round1 | full | 35.0 | 40.3 | 5.2 |
| release-probe-2 | Release gate-round1 | full | 24.7 | 49.7 | 24.9 |
| release-phases-1 | Release gate-round1 | full | 26.5 | 30.9 | 4.4 |

Facts proven by measurement:
1. stdout EOF comes 25–68 ms after stdin close in BOTH configs — long before the Debug process exits (~3 s).
   The client's 2 s post-EOF clock (engine_client.cpp:333) therefore expires ~0.7–1.3 s before the real Debug exit
   → GetExitCodeProcess = 259 (STILL_ACTIVE) → «The engine exited (code 259).» at :159.
2. The Debug engine exits BY ITSELF with code 0, 2.66–3.25 s after stdin close (6/6 full runs). NOT a hang,
   NOT terminated. Release exits 31–50 ms after close (eof→exit 4–25 ms) → the 2 s wait always succeeds.
3. Proportional to the number of INTENTIONALLY LEAKED crashed instances: 0 leaks → eof→exit 120 ms;
   1 leak → 1193 ms (< 2 s → pass); 2 leaks → 2660–3250 ms (> 2 s → 259). This is exactly why the other Debug
   cases pass (restart/bounded-reset leak ONE instance; the throw case unwinds cleanly under /EHsc and releases).
4. Last stdout line (ticker stateEvent) at close+21..32 ms; EOF shortly after → the protocol handle
   (g_protocol, the _dup of stdout kept by keepStdoutForProtocol, main.cpp:113-122) is closed during the EXE's
   CRT exit uninitialization, BEFORE the slow part of teardown (no explicit fclose exists anywhere — grep verified).

## Step 3 — blocking thread + stacks (captured while STILL ACTIVE, past the client's 2 s budget)

debug-probe-1, eof+2.2 s (the exact moment the client reports 259) — MAIN thread (the only one doing work;
3 others already reaped by ExitProcess, SuspendThread error 5):

    ntdll!NtWaitForSingleObject
    KERNELBASE!WaitForSingleObjectEx / OutputDebugStringA / OutputDebugStringW
    Core Test Crash!__acrt_OutputDebugStringA      ucrt\appcrt\internal\outputdebugstringa.cpp:31
    Core Test Crash!_VCrtDbgReportA                ucrt\appcrt\misc\dbgrptt.cpp:412
    Core Test Crash!_CrtDbgReport                  ucrt\appcrt\misc\dbgrpt.cpp:263
    Core Test Crash!dump_all_object_since_nolock   ucrt\appcrt\heap\debug_heap.cpp:1652
    Core Test Crash!_CrtMemDumpAllObjectsSince     debug_heap.cpp:1696
    Core Test Crash!_CrtDumpMemoryLeaks            debug_heap.cpp:1723
    Core Test Crash!report_memory_leaks            ucrt\appcrt\internal\initialization.cpp:211
    Core Test Crash!__acrt_execute_uninitializers  shared_initialization.cpp:64
    Core Test Crash!__acrt_uninitialize            initialization.cpp:314
    Core Test Crash!__scrt_uninitialize_crt        vcstartup\src\utility\utility.cpp:221
    Core Test Crash!dllmain_crt_process_detach     vcstartup\src\startup\dll_dllmain.cpp:195
    Core Test Crash!_DllMainCRTStartup             dll_dllmain.cpp:335
    ntdll!LdrShutdownProcess  →  ntdll!RtlExitUserProcess  →  KERNEL32!ExitProcess
    core-engine!exit_or_terminate_process          ucrt\appcrt\startup\exit.cpp:142
    core-engine!common_exit                        exit.cpp:288
    core-engine!exit                               exit.cpp:302
    core-engine!__scrt_common_main_seh             exe_common.inl:295  →  mainCRTStartup

debug-deep-1 full captures at eof+0.4 s / +1.5 s / +2.2 s: the SAME main thread is still earlier in the chain —
inside CORE-ENGINE'S OWN CRT leak dump (before ExitProcess):

    core-engine!common_exit                        exit.cpp:286
    core-engine!__scrt_uninitialize_crt            utility.cpp:221
    core-engine!__acrt_uninitialize                initialization.cpp:314
    core-engine!__acrt_execute_uninitializers      shared_initialization.cpp:64
    core-engine!report_memory_leaks                initialization.cpp:211
    core-engine!_CrtDumpMemoryLeaks                debug_heap.cpp:1723
    core-engine!_CrtMemDumpAllObjectsSince         debug_heap.cpp:1696
    core-engine!dump_all_object_since_nolock       debug_heap.cpp:1652
    core-engine!_CrtDbgReport                      dbgrpt.cpp:263
    core-engine!_VCrtDbgReportA                    dbgrptt.cpp:293 (vsnprintf_s formatting) / :412 (OutputDebugString)
    → KERNELBASE!OutputDebugStringA: OpenFileMappingW(DBWIN_BUFFER) / WaitForSingleObjectEx / ReleaseMutex
      + RaiseException(DBG_PRINTEXCEPTION_C)/RtlUnwindEx (the SEH debugger probe, unwound when no debugger)

At eof+2.6 s the EXE dump is done and the main thread is in the PLUGIN DLL's detach dump
(Core Test Crash!dump_all_object_since_nolock debug_heap.cpp:1672 — the per-block CONTENT HEX DUMP).
100 ms phase sampling (debug-phases-1, debug-deep-1) shows this OutputDebugString/vsprintf/SEH cycle for the
ENTIRE close→exit window. Other threads: 3 thread-pool workers idle in NtWaitForWorkViaWorkerFactory; no audio
thread (null backend, stopped), no JUCE dispatch loop (already returned), no watchdog, no plugin lock wait.

=> The blocker is the MAIN thread, in the Debug CRT's automatic exit-time memory-leak dump
   (_CrtDumpMemoryLeaks → per-block _CrtDbgReport → OutputDebugStringA), run TWICE: once by core-engine.exe's own
   CRT during common_exit (the leaked host-side JUCE wrappers etc.), once by each leaked plugin DLL's CRT during
   DLL_PROCESS_DETACH (the leaked plugin instances + their internals). Each block costs ~0.3–1 ms (DBWIN mutex
   sync + SEH debugger probe + vsnprintf formatting + hex dump); the two leaked instances' allocation graphs are
   thousands of blocks → ~2.7–3.3 s total. Release CRT has no debug heap → no dump → 5–25 ms.

## Step 4 — code correlation (read, and matches the tested builds)

- Leak policy (WHY the blocks exist): juce_host.cpp:100-122 ~JuceProcessor — a crashed instance is NEVER released
  or deleted: instance_.release() then return (comment :106-114: SEH catch runs no destructors, JUCE's processMutex
  stays held forever; releasing would deadlock). The raw juce::AudioPluginInstance (host-side wrapper, engine heap)
  and the plugin's own processor (plugin DLL heap) both stay allocated → the Debug CRTs report them at exit.
  This leak is INTENTIONAL and correct (M1 fix); the dump is its Debug-only side effect.
- serve exit path: main.cpp:162-176 stdin EOF → running=false, ticker.join(), Server dtor (server.cpp:235
  output_->stop()) → runWithMessageLoop (juce_host.cpp:640-649) stops the dispatch loop → ~JuceHost (:362-366,
  releases the HEALTHY synth on the message thread) → ~Runtime (:638, JUCE shutdown) → main returns → CRT exit.
  All of that is the FAST part (close→EOF 25–68 ms, matches debug-clean-1's 120 ms total).
- stdout handle: keepStdoutForProtocol (main.cpp:113-122) _dup's stdout into g_protocol and _dup2's stderr over
  fd 1 at STARTUP; the pipe's write end lives only in g_protocol. No explicit fclose anywhere (grep). The UCRT's
  exit uninitialization closes it (EOF at +25–68 ms) BEFORE report_memory_leaks and ExitProcess's DLL detaches.
- Client: engine_client.cpp:333-338 — after stdout EOF: WaitForSingleObject(process, 2000) then GetExitCodeProcess
  → 259 if still alive → disconnect reason «The engine exited (code 259).» → test :159 fails. close() (:160-179)
  waits closeTimeout and TerminateProcess(1) on timeout; the test sets closeTimeout=10000 (:34) so close() does NOT
  terminate (wall ~6 s proves it); the READER's hardcoded 2000 is what misreports.
- APP default closeTimeout = 2000 ms (engine_client.hpp:46; EngineSession.cpp:509-517 uses defaults) → in the Debug
  APP, closing a session with ≥2 leaked instances TERMINATES the engine at 2 s (mid-leak-dump), exit code 1,
  before its natural ~3 s exit. Confirmed in code + measured engine exit time (app behavior itself not run).
- Graph leaks are NOT involved: engine.cpp:238 leakedGraphs only when the retire queue is full (stays 0 here);
  graph.cpp:285 failedPlugins note. The retry rebuild retires graphs normally.
- Working tree vs builds: s1-fix/gate-round1 = a9a0384 + round-1 fixes. Current tree ALSO has round-2 work
  (S1 export refusal in server.cpp renderProject, S3 state validation in juce_host.cpp Impl::create, EngineSession/
  MainComponent/graph/render_job changes). NONE of it touches the shutdown path cited above (diffs checked);
  engine_client.cpp and main.cpp are unmodified. Conclusions apply to the tested builds and to the current tree's
  shutdown code alike.

## Step 5 — VERDICT: PROVEN (measurement + stacks + code correlation)

ENV-03 is a CLIENT-side measurement artifact over a slow-but-finite ENGINE teardown:
- The engine, after the crash/retry scenario, exits BY ITSELF with code 0 in Debug ~2.7–3.3 s after stdin close.
- The delay is the Debug CRT's automatic exit-time leak dump of the INTENTIONALLY leaked crashed instances
  (EXE CRT during common_exit + plugin DLL CRT during DLL_PROCESS_DETACH), ~1.2 s per leaked instance.
- stdout EOF happens at +25–68 ms (CRT exit closes g_protocol before the dumps), so the client's FIXED 2 s
  post-EOF wait (engine_client.cpp:333) expires before the real exit → GetExitCodeProcess=259 → the test fails.
- 1 leaked instance ≈ 1.2 s < 2 s → the other Debug cases pass; 0 leaks ≈ 0.12 s; Release ≈ 5–25 ms (no debug heap).
- NOT a hang, NOT a plugin-lock deadlock, NOT the audio device, NOT the watchdog. No terminate needed or observed.
- The historical one-off Debug PASS (s4-fix) is consistent with the measured 2.66–3.25 s variance around the 2 s
  boundary (timing-sensitive), not reproduced here.

### Fix options (PROPOSED ONLY — no repo edits made; implementation needs separate authorization)

A. MINIMAL, client (recommended): engine_client.cpp:333 — replace the hardcoded 2000 with the client's own
   closeTimeout budget: WaitForSingleObject(process, (DWORD)options_.closeTimeout.count()) (or max(2000, closeTimeout)).
   Rationale: stdout EOF means the engine has begun exiting; the wait for "did it exit by itself" should use the same
   bounded budget close() uses for termination. Test (closeTimeout=10 s) → engine exits ~3 s → reason «code 0» → green.
   No existing test pins the 2 s constant (grep: only engine_process_tests/state_recovery_tests use EngineClient,
   both set 10000). Trade-off: if stdout ever EOFs while the engine truly hangs, the disconnect reason arrives up to
   closeTimeout later (bounded; close() terminates on the same budget anyway). Unexpected engine death is unaffected
   (process already signaled → wait returns immediately).
A2. APP consistency (optional decision): raise EngineOptions::closeTimeout default (engine_client.hpp:46) 2000 →
   e.g. 10000 so the Debug app also lets the engine exit by itself instead of terminating it mid-dump at 2 s.
   Owner/coordinator decision: how long the app may wait on a genuinely hung engine before terminating.
B. ENGINE/TEST-PLUGINS (optional, makes Debug shutdown fast): disable the automatic Debug CRT leak check at exit —
   _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & ~_CRTDBG_LEAK_CHECK_DF) under _DEBUG in serve startup AND in
   the self-test plugins (their CRTs dump at DLL detach). Removes ~2.9 s of pure noise (the leaks are BY DESIGN;
   the dump re-reports them every session end). Trade-off: loses automatic Debug leak reporting there — keep explicit
   _CrtDumpMemoryLeaks checkpoints in dedicated leak tests if wanted. Third-party Debug-built plugins could still
   dump (not ours to control) → A remains the robust guard. Requires rebuilding engine + test plugins.
C. NOT recommended: self-terminate after serve teardown (TerminateProcess(GetCurrentProcess(), 0)) to skip CRT/DLL
   cleanup — abrupt, skips stderr flush and DLL detach, larger semantic change; unnecessary given A/B.

Recommended package: A (+ A2 as a decision) as the minimal correct fix; B as an optional follow-up for Debug UX.

### Red/green acceptance plan (for the authorized fix round)

RED (already measured on the current state, no fix): Debug s1-fix engine_process_tests "crashes in processBlock"
3/3 FAIL «code 259» at :159 — D:\Core-build\env03\logs\debug-crash-run{1,2,3}.log.
GREEN after A: rebuild the Debug integrated build; engine_process_tests FULL suite Debug ×3 → every case passes,
the crash case reports «The engine exited (code 0).»; Release full suite stays green (gate-round1 baseline 25/25);
state_recovery_tests Debug+Release (same closeTimeout). Probe re-measure: engine timeline unchanged (~3 s),
client-visible reason now code 0; close() must NOT terminate (test wall ~6 s, not ~14 s).
Hang-path guard: an engine that does NOT exit must still be terminated at closeTimeout with reason «code 1» —
exercise via killForTest/a stuck-engine probe variant; add a regression test if none exists (fix-round scope).
No check weakened: :159 still demands code 0 (self-exit, not termination). If B is also taken: Debug eof→exit
drops to ~120 ms (clean-control baseline) and the crash case passes with margin.

## Limitations

- Probe drives raw pipes mimicking EngineClient (identical handle-list setup), not the linked lib; protocol behavior
  verified equivalent (hello/commands/events; 0 bad lines in all runs). App-level close() behavior with the 2000 ms
  default is confirmed in code + measured engine exit times, not by running the desktop app.
- Stack capture suspends threads; the deep/phases runs are slightly perturbed (clean timing runs are not).
- No rebuilds: binaries used as delivered (s1-fix Debug, gate-round1 Release, both a9a0384+round-1). Round-2
  working-tree changes are NOT in these builds (shutdown code identical, diffs checked).
- The leak dump's block inventory was not captured (it goes to OutputDebugString, not stderr); the per-leak
  proportionality is proven by the 0/1/2-leak measurements instead. Per-block OutputDebugString cost not isolated.
- Some unwound SEH frames in system DLLs show cosmetic symbol misattribution (e.g. RtlWow64SuspendThread);
  Core/JUCE/plugin frames resolve to file:line via the build PDBs (pattern proven by core-review stack_probe).
- Pre-existing orphan core-engine pid 40608 (started 19/11 06:51, before this investigation) left untouched.

## Log inventory (D:\Core-build\env03\logs\)

- debug-crash-run{1,2,3}.log — test repro Debug 3/3 FAIL 259; release-crash-run{1,2,3}.log — Release 3/3 PASS.
- debug-restart-the-component.log, debug-bounded-reset.log, debug-throws-while-playing.log — Debug PASS.
- debug-probe-{1,2,3}.log, debug-probe-4-full-oldbinary.log, debug-phases-1.log, debug-deep-1.log — Debug full timelines
  (+stderr/stdout companions); debug-deep-1.log holds the eof+0.4/1.5/2.2/2.6 s full stacks.
- debug-single-1.log (1 leak, 1193 ms), debug-clean-1.log (0 leaks, 120 ms) — proportionality controls.
- release-probe-{1,2}.log, release-phases-1.log — Release timelines (eof→exit 4–25 ms).
- stuck-39060-stacks.log, probe-self-stacks.log — first-run probe-bug diagnosis (engine healthy, probe reader dead).
- Probe sources: D:\Core-build\env03\probe\{env03_probe.cpp, stacks_only.cpp, build-probe.bat}.

