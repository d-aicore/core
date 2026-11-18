# Gate: a unique temp bat per invocation (concurrency fix)

2026-11-18, PM team session. Branch `ci/gate-unique-bat`, fix commit `fffbb41`. Owner OK'd the fix 2026-11-18.

## Red — measured incident (concurrent gates clobbered the shared bat)

`ci/gate.ps1` wrote a fixed `%TEMP%\core-gate.bat` (line 37). A parallel coordinator stream (SUR) running its
own gate overwrote that file **mid-run** while the flawless-batch gate's `cmd` was reading it:

- `gate-run1-corrupted.log`: *"'e' is not recognized as an internal or external command"* — the bat was
  half-rewritten while cmd parsed it.
- `gate-run1-resumo.txt`: false **VERMELHO** (corruption, not a real test failure).
- `gate-run2-corrupted.log`: run 2's `ctest` even executed against **SUR's build dir**, not this branch.

Both runs were **discarded** — not measurements of the batch branch. Evidence preserved in
`D:\Core-build\flawless-batch-evidence\` (`gate-run1-corrupted.log`, `gate-run2-corrupted.log`,
`gate-run1-resumo.txt`, `gate-run2-console.log`).

## Fix

`gate.ps1` now writes `core-gate-<guid>.bat` per invocation (`[guid]::NewGuid()`) and removes it afterwards
(`Remove-Item`). Any number of concurrent gates use distinct files — nothing shared to overwrite. The
machine-wide `D:\Core-build\GATE-LOCK` convention (PM side) still serializes CPU; the unique bat makes the
gate correctness independent of whether another stream honors the lock.

## Green — measured concurrency proof

Two gates launched **concurrently** from `ci-gate-unique` @ `fffbb41`, `-Jobs 3`, separate build dirs:

| Gate | Build dir | Verdict | Tests | Time |
|---|---|---|---|---|
| A | `D:/Core-build/gate-proof-a` | **VERDE** | 17/17 | 564 s |
| B | `D:/Core-build/gate-proof-b` | **VERDE** | 17/17 | 562 s |

Both `Codigo: ...ci-gate-unique @ fffbb41`, no dirty flag, distinct dirs, no corruption. The unique bats were
cleaned up (no `core-gate-*.bat` left in `%TEMP%`). Evidence: `D:/Core-build/gate-proof-a/gate-resumo.txt`,
`D:/Core-build/gate-proof-b/gate-resumo.txt`.

**Self-correction (recorded):** the first proof attempt was *vacuous* — Git Bash mangled `ci\gate.ps1` into
`cigate.ps1` (backslash escape) and `| tail` masked the real exit code, so it reported exit 0 without running.
Caught because no `gate-resumo.txt` existed; redone with forward-slash paths and no tail. Lesson for all
streams: invoke the gate with `ci/gate.ps1` (forward slashes) and read `gate-resumo.txt`, never pipe to tail.

## Propagation

`fffbb41` cherry-picked into `flawless-batch`, `flawless-packaging`, `flawless-tempo`, `flawless-timeline`
(clean worktrees). `flawless-deadcode` continuation cherry-picks it itself; `flawless-crash` pending (dirty
while fix D is in progress). The SUR coordinator is to honor `GATE-LOCK` (owner bridge); the unique bat
protects Core gates regardless.

## Proposed ledger line (for the scribe — NOT written to ESTADO.md)

"2026-11-18 — `ci/gate.ps1` usa um .bat temporário único por invocação (`core-gate-<guid>.bat`), eliminando a
colisão de gates concorrentes que sobrescrevia `core-gate.bat` a meio da execução (incidente medido: dois gates
do lote BUG-SWEEP corrompidos pelo stream SUR, descartados). Prova: dois gates concorrentes VERDE 17/17 @fffbb41
(`gate-proof-a`/`b`). Fix `fffbb41`, branch `ci/gate-unique-bat`; owner OK 2026-11-18." Fonte: este relatório +
evidência em `D:/Core-build/gate-proof-{a,b}/`. D-number pendente do escrivão.