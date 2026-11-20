# Relatório — 2026-11-20 — S5: samples do utilizador protegidos na atualização e desinstalação

Delivery: DSH lead session `a69b320fbfab`, agents `s5-red` (vermelho, fluxo real do instalador)
and `s5-fix` (correção + regressão permanente), coordinator (revisão do diff, retomada do build),
2026-11-20 Europe/Lisbon. Owner authorization: S-round dispatch 2026-11-20, item S5 (DATA
priority: «A proteção tem de cobrir também um sample anteriormente listado no manifesto, porque
esse caminho pode continuar marcado para remoção na desinstalação»). Q-consultation of the
original plan pending (commit `1c6238b` unreachable — see the S11 report's hunt record); the
dispatch's explicit contract governs.

## O que foi medido

- Repositório `d-aicore/core`, `main @ a9a038430d781959829f8510067ce59afffaad62` (= origin/main).
  Local build-affecting changes preserved: CMakeLists.txt (ENV-01 + D113 hunks, uncommitted).
- Ambiente: Windows 11 x64, MSVC 19.51.36260, CMake 4.4.4, Ninja 1.13.2; vcvars64 + VSLANG=1033 +
  chcp 65001 (ci/gate.ps1 conventions). Instalador real `Core Setup.exe` (Debug, built at this
  SHA), silent flow `--install/--uninstall <folder> --no-shortcuts --samples <lib>` — per-user,
  HKCU-only, **no elevation needed**; all invocations exit 0/1 as designed.
- Testes executados: matriz manual de 7 casos (vermelho, s5-red) + regressão permanente
  `installer_samples` (70 checks: vermelho 29 falhas no não-corrigido → verde 70/70 ×2 no
  corrigido) + segunda confirmação manual com o binário corrigido (8× exit 0, hashes cruzados).
- Limitações: fluxo silent medido, GUI não clicado (partilha `copySamples`/`uninstall` — inferência
  por leitura); versão B não construída (versão compile-time; reinstalar A exercita o mesmo
  ramo); fixtures sintéticos ~KB, não a biblioteca ~7 GB (decisão por-ficheiro, independente de
  escala; o reinstalar passa a ler cada ficheiro listado — o custo da correção); self-delete do
  Uninstall.exe a correr do próprio alvo não exercitado; `--no-shortcuts` (sem registo/atalhos).

## Resultados

| ID | Problema / resultado | Evidência | Classificação | Estado |
| --- | --- | --- | --- | --- |
| S5-RED-i | Sample distribuído EDITADO pelo utilizador (+256 B) → reinstalar sobrescreve com o source (hash fac4d371→b294a261) | `s5-red-manual-matrix-notes.md` caso (i), logs 00–17 | comprovado por teste | vermelho medido (antes) |
| S5-RED-ii | Ficheiro DO UTILIZADOR em caminho distribuído → sobrescrito E apagado na desinstalação | caso (ii) | comprovado por teste | vermelho medido |
| S5-RED-iv | Ficheiro do utilizador com mesmo tamanho+data em caminho anteriormente listado → cópia saltada mas FICA no manifesto → desinstalação APAGA-O | caso (iv) + isolamento (vii) | comprovado por teste | vermelho medido |
| S5-RED-controls | (iii) intacto: saltado/listado/removido ✓; (v) nunca-listado com match exato: não listado, sobrevive (Audit-27) ✓; (vi) caminho não-distribuído: intacto ✓ | casos iii/v/vi | comprovado por teste | comportamento correto preservado |
| S5-FIX | Propriedade por CONTEÚDO (sha256): manifesto `<rel>\t<hash do que o Core instalou>`; linhas simples = LEGACY (prova contra o source, nunca assumida). Cópia: hash(dest)=instalado → cópia do Core (atualiza se source difere); hash≠ → conteúdo do utilizador → SKIP + entrada removida + mensagem «Kept your file…». Desinstalação: re-valida hash antes de apagar; legacy sem source disponível → NUNCA apaga (conservador); source==alvo conta como indisponível; falha de delete mantém entrada para retry | `app/installer/Setup.cpp` (Sha256 :74, sha256OfFile :132, entradas :148-163, copySamples :169/decisão :202/mensagem :213, removeSamples :233/guard :237/mensagem :257, uninstall :376, GUI :441, CLI :530); diff revisto pelo coordenador | confirmado no código + comprovado por teste | corrigido (por commitar) |
| S5-REG | Regressão permanente `tests/installer_samples.cmake` (7 casos + sub-casos revalidate/legacy-nosrc, 70 checks, fixtures gerados, fluxo silent real, 5 alvos temp) registada em tests/CMakeLists.txt:119-130 (guard WIN32+CORE_BUILD_APP+TARGET CoreSetup, LABELS merge, RUN_SERIAL, TIMEOUT 300). **Vermelho não-vacuo: 29/70 falhas no não-corrigido** (i/ii/iv/vii/revalidate/nosrc), controlos iii/v/vi passam. **Verde: 70/70 ×2, exit 0** + matriz manual cruzada (8× exit 0) | `red-run-final.txt`, `green-run.txt`, `manual-matrix-fixed.txt`, `merge-list.txt` | comprovado por teste | verde |
| S5-COUNTS | ctest -N -L merge: **22 testes = 21 existentes + installer_samples**; nenhum removido/enfraquecido; diff de tests/CMakeLists.txt = +12 aditivo | `merge-list.txt`; git diff | comprovado por execução | validado |

## Alterações

- `app/installer/Setup.cpp` — +189/−28 (SHA-256 auto-contido, espelho da implementação provada de
  app/domain/src/sha256.cpp; juce_cryptography/core_domain não estão no link set do instalador,
  sem alteração CMake); decisão de cópia e remoção por conteúdo; mensagens English nomeiam cada
  ficheiro preservado; exit codes inalterados; CLI `--uninstall` aceita `--samples` para a prova
  legacy.
- `tests/installer_samples.cmake` — novo (70 checks).
- `tests/CMakeLists.txt` — registo aditivo (+12).
- Este relatório + índice + `evidencia/2026-11-20-s5-installer-sample-protection/` (6 ficheiros).
- `ESTADO.md` não editado (D72/D80) — linhas propostas abaixo.
- Fora do repo: `D:\Core-build\s5-installer\` (matriz vermelha manual, logs 00–17),
  `D:\Core-build\s5-fix\` (builds, red/green, matriz manual corrigida), notas em
  `D:\Core-build\coordinator-round2\s5-*.md`.

## Verificação

- Vermelho (antes, binário não-corrigido @ a9a0384): `ctest --test-dir D:\Core-build\s5-fix\build -R installer_samples -V` → **29 de 70 checks FAILED**, exit 8; casos i/ii (sobrescrita), iv/vii/revalidate/nosrc (apagamento pelo manifesto) falham; iii/v/vi passam. Matriz manual original: 7 casos, hashes antes/depois por ficheiro.
- Verde (depois): mesmo comando → **all 70 checks passed**, exit 0, duas corridas; segunda confirmação manual (método s5-red, binário corrigido) → 8× exit 0, hashes cruzam exatamente com o teste cmake; manifesto pós-reinstalar = linha hashada; pós-desinstalar = só conteúdo do utilizador.
- Contagens: merge 22 testes listados (21 + novo); nenhuma suite existente alterada.
- Nota de coordenação: o build do agente morreu com o turno @ [321/357]; o coordenador retomou-o (rc=0) — registado nas notas. O diff final foi revisto pelo coordenador (decisão de cópia, remoção, guard self-source, retry, Audit-27).

## Pendências e passagem

- **Commit**: correção S5 + regressão aguardam decisão do dono (nada commitado; D48/D50).
- **Gate da revisão integrada**: pendente (coordenador, após S1/S4); este relatório não declara gate completo.
- **Recomendações registadas (não implementadas)**: reter backup do export também no caminho de SUCESSO (decisão de produto separada, do achado S1); o CLI render usa o `.part` legacy fixo (follow-up SWEEP-02, do achado S1).
- **Linhas propostas para ESTADO.md (o escrivão escreve, D72/D80):**
  - *Estado medido — S5 samples do utilizador (2026-11-20, a9a0384 + correção local por commitar)*:
    vermelho medido no fluxo real do instalador (editado sobrescrito; ficheiro do utilizador em
    caminho distribuído sobrescrito+apagado; manifesto apaga conteúdo do utilizador mesmo com
    cópia saltada — casos i/ii/iv/vii); correção por propriedade de conteúdo (sha256 no manifesto,
    legacy provado contra o source, sem source nunca apaga, re-validação na remoção, Audit-27
    preservado); regressão permanente installer_samples 70 checks: vermelho 29 falhas → verde
    70/70 ×2 + matriz manual cruzada; merge 22 testes (21+1), nada removido. Setup.cpp +189/−28.
  - *Pendência*: commit da correção S5 (decisão do dono); GUI não clicado (inferência); versão B
    não construída; biblioteca real ~7 GB não exercitada (decisão por-ficheiro).
  - *Referência*: docs/relatorios/2026-11-20-s5-installer-sample-protection.md + evidência (6 ficheiros).
- **Retoma**: SHA a9a0384 + hunks locais; build D:\Core-build\s5-fix\build (CoreSetup corrigido);
  próxima ação = integrar S1/S4 e correr o gate merge+desktop na revisão integrada (version==sha).
