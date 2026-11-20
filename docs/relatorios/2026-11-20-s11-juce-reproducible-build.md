# Relatório — 2026-11-20 — S11: build JUCE reproduzível (patch D113 aplicado/verificado pelo CMake)

Delivery: DSH lead session `a69b320fbfab`, agent `s11-fix` (implementation + cases 1/2/3/5),
coordinator (case-4 Release build resume + behaviour proofs), 2026-11-20 Europe/Lisbon.
Owner authorization: S-round dispatch 2026-11-20, item S11 («aplicar o patch de forma idempotente
à cópia descarregada pelo build e verificar a cópia local quando esta é fornecida… falhar cedo…
A verificação deve comprovar a aplicação do patch completo»). Priority: dados > tempo real >
performance > compatibilidade (D54). Q7 of the original continuation plan could NOT be consulted:
the plan commit `1c6238b` is unreachable locally and on the remote (exhaustive search recorded in
`evidencia/.../plan-hunt-closure.md`); the dispatch's explicit contract governs as the current
owner instruction (AGENTS.md). Reconcile by reference if the plan surfaces.

## O que foi medido

- Repositório `d-aicore/core`, `main @ a9a038430d781959829f8510067ce59afffaad62` (= origin/main).
  Local build-affecting changes: CMakeLists.txt (ENV-01 sanitizer wiring, uncommitted, preserved
  verbatim as a separate hunk) + the new D113 hunk (+56 lines). No other product file touched.
- Ambiente: Windows 11 x64, MSVC 19.51.36260 (VS 18 Build Tools), CMake 4.4.4, Ninja 1.13.2;
  vcvars64 + VSLANG=1033 + chcp 65001 per ci/gate.ps1 (MSVC emits Portuguese when English
  resources are absent — known AGENTS.md limitation, does not affect evidence).
- JUCE: pinned 9.0.5, URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz,
  URL_HASH SHA256=d92207f51ba728d3d195b6d5e93ecd049717a915fc250ff6844c32ea7272315a
  (commit d60b7a78030661574a615b586d9325ba7a7fa7dd). Local patched copy: D:\Core-deps\JUCE-9.0.5
  (extracted, no .git). Patch: ci/patches/juce-failed-plugin-restart.patch (14 hunks, 2 files
  under modules/juce_audio_processors_headless/).
- Testes executados: os 5 casos de aceitação abaixo + engine_process_tests (Release) casos
  «restart the component» ×3 e «crashes in processBlock» ×3 + ctest cli_plugin_crash_render.
- Limitações: Q7 pendente (plano não localizável); o gate completo da revisão integrada ainda
  não correu (coordenador, após S1/S5); Linux não prova Windows e vice-versa; o caso negativo
  mede a recusa do CMake, não um build completo sem patch (que morre em [19/555] — medido antes).

## Resultados

| ID | Problema / resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| S11-RED-1 | Fetch limpo (deps+build novos, sem cópia local): JUCE 9.0.5 descarregado, hash verificado, pristine → build morre em juce_host.cpp(136) C2039 'markCrashedByHost' não é membro de VST3Client, rc=2 @ [19/555] | `red-clean-fetch-build.txt`; deps/juce-src grep=0; configure hash OK | comprovado por teste | vermelho medido (antes) |
| S11-RED-2 | Controlo negativo: cópia local SEM patch (robocopy 5194 ficheiros + `git apply -R` rc=0, grep=0, `--check -R` rc=1 = reversão completa provada) → mesmo C2039, rc=2 @ [19/555]; include paths apontam para a cópia | `red-local-unpatched-build.txt`, `s11-negctrl-notes.md` | comprovado por teste | vermelho medido (antes) |
| S11-FIX | CMake aplica o patch à cópia descarregada (idempotente) e VERIFICA a cópia local (falha cedo, nunca a modifica). Prova de patch completo = `git apply --check -R` rc=0 ⇔ todos os 14 hunks presentes; pós-aplicação re-verifica e recusa parcial | `CMakeLists.txt:68-123` (hunk D113, +56); ENV-01 preservado verbatim (2 hunks, +69 total, 0 deleções) | confirmado no código | implementado (por commitar) |
| S11-C1 | Caso 1 — fetch limpo + fix: re-configure do dir vermelho rc=0 «Applied D113 … (full application verified)»; reverse-check independente rc=0, grep=2; `--target core_plugin_host` rc=0 (juce_host.cpp.obj compila, lib liga) | `s11-fix-notes.md` caso 1; configure/build logs | comprovado por teste | verde |
| S11-C2 | Caso 2 — local sem patch + fix: configure rc=1 FATAL_ERROR claro @ CMakeLists.txt:92 (nomeia patch/dir/rc/requisito; «The local copy was NOT modified»); cópia provada intacta depois (reverse-check rc=1, grep=0) | `s11-fix-notes.md` caso 2 | comprovado por teste | verde (recusa correta) |
| S11-C3 | Caso 3 — local COM patch (D:/Core-deps): configure rc=0 «already carries the full … (verified)», só verificação (nunca git apply); deps intactos; `--target core_plugin_host` rc=0 | `s11-fix-notes.md` caso 3 | comprovado por teste | verde |
| S11-C4 | Caso 4 — Release fresco (deps+build novos, download pristine verificado por urlinfo hash == pin) + fix: configure aplica+verifica; build completo rc=0 (495 passos incrementais após retomada); COMPORTAMENTO: «restart the component» 3/3 exit 0 (1 teste/17 verif/0 falhas — regressão D113), «crashes in processBlock» 3/3 exit 0 (1/25/0), cli_plugin_crash_render 1/1 Passed | `proof-summary.txt`, `restart-run1.txt`, `crash-run1.txt`, `cli-crash-render.txt`; build-full.txt | comprovado por teste | verde |
| S11-C5 | Caso 5 — idempotência: re-configure do caso 1 rc=0, reverse-check acerta primeiro («already carries…»), SEM reaplicação, grep=2 | `case5-idempotent-configure.txt` | comprovado por teste | verde |

**Aceitação S11 do dono: os 5 casos pedidos estão provados** (build Windows com dependência e
diretório novos; build com JUCE local corrigido; controlo negativo com cópia local sem patch;
teste de crash/restart VST3; revisão do Core/JUCE e resultado do patch registados por caso).

## Alterações

- `CMakeLists.txt` — hunk D113 (+56 linhas) após `FetchContent_MakeAvailable(JUCE)` dentro de
  `CORE_BUILD_APP AND WIN32`: `find_program(git)` obrigatório; `git apply --check -R` como prova
  de patch completo; cópia local sem patch → FATAL_ERROR cedo (nunca modificada); cópia
  descarregada → `git apply` + re-verificação + recusa de aplicação parcial; idempotente em
  re-configures. O hunk ENV-01 (sanitizer targets, por commitar da entrega de ambiente) fica
  preservado verbatim — duas alterações lógicas separadas no mesmo ficheiro.
- Este relatório + linha no índice + `evidencia/2026-11-20-s11-juce-reproducible-build/` (11 ficheiros).
- Fora do repo: dirs de prova `D:\Core-build\s11-{clean,local-unpatched,local-patched,release-fixed,juce-unpatched}`,
  logs `D:\Core-build\s11-release-proof\`, notas `D:\Core-build\coordinator-round2\s11-*.md`.
- `ESTADO.md` não editado (D72/D80) — linhas propostas abaixo.

## Verificação

Comandos repetíveis (vcvars64 + VSLANG=1033 + chcp 65001, Ninja):
- Vermelhos (antes do fix): `cmake -S D:\CORE\CORE -B D:\Core-build\s11-clean\build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_TESTS=ON -DFETCHCONTENT_BASE_DIR=D:/Core-build/s11-clean/deps` + `cmake --build … --parallel 4` → rc=2, C2039 juce_host.cpp(136). Idem com `-DFETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-build/s11-juce-unpatched`.
- Verdes (depois do fix): mesmos comandos → configure aplica/verifica o patch; `--target core_plugin_host` rc=0. Release fresco: `-DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=D:/Core-build/s11-release-fixed/deps` → build completo rc=0; `tests\engine_process_tests.exe "restart the component"` ×3 → 0/0/0 (1/17/0); `"crashes in processBlock"` ×3 → 0/0/0 (1/25/0); `ctest -R cli_plugin_crash_render -V` → 1/1.
- Nota de coordenação: o build Release em background do agente morreu com o turno dele @ [63/555];
  o coordenador retomou-o (incremental, rc=0) e correu as provas de comportamento — registado
  nas notas. Nenhum resultado foi aceite sem o SHA/config corretos.

## Pendências e passagem

- **Commit**: esta alteração (hunk D113) + ENV-01 + relatórios aguardam a decisão do dono
  (entrega de tooling separada em `D:\Core-build\core-review\tooling-delivery\`; o hunk D113 é
  uma alteração lógica distinta — pode ser commit separado). Sem autorização verbatim, nada commitado.
- **Q7 do plano original**: não consultável (plano não localizável localmente nem no remote —
  `plan-hunt-closure.md` na evidência). Se o plano aparecer, reconciliar por referência.
- **Gate da revisão integrada**: o coordenador correrá o gate merge+desktop na revisão que
  integrar S11+S1+S5 (e restantes), com version==sha; este relatório não declara gate completo.
- **Ronda S**: S1 (vermelho medido, casos a–d; e em curso) e S5 (vermelho medido, regressão
  permanente escrita, correção em curso) seguem; S4 vermelho confirmado (NaN/±Inf raw, overflow
  float64→float, /fp:fast incondicional em engine/CMakeLists.txt:9 — a regressão da correção
  deve correr em Debug E Release).
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — S11 JUCE reproduzível (2026-11-20, a9a0384 + hunks locais por commitar)*:
    vermelho medido antes (fetch limpo e cópia local sem patch → C2039 markCrashedByHost
    juce_host.cpp:136, rc=2 @[19/555]); correção CMake (+56 linhas): patch D113 aplicado
    idempotentemente à cópia descarregada, cópia local verificada (falha cedo, nunca modificada),
    prova de patch completo por `git apply --check -R`; 5 casos verdes incl. Release fresco
    (hash pin verificado) com crash/restart 3/3 (17 verif) + processBlock-crash 3/3 (25 verif) +
    cli_plugin_crash_render 1/1. Falso-verde de fetch novo fechado ao nível do build.
  - *Pendência — Q7/plano original*: commit 1c6238b não localizável (300+ repos, dangling, bare,
    remote só main@a9a0384, logs Qoder/DSH sem decoder); consulta Q1–Q8 fica pendente; os
    contratos explícitos do dispatch do dono governam a ronda 1.
  - *Referência*: docs/relatorios/2026-11-20-s11-juce-reproducible-build.md + evidência (11 ficheiros).
- **Retoma**: SHA a9a0384 + hunks CMake (D113 + ENV-01); provas em `D:\Core-build\s11-*` e
  `s11-release-proof\`; próxima ação = integrar S1/S5/S4 e correr o gate na revisão integrada.
