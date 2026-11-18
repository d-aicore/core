# Report — 2026-11-18 — the five maps in English

## What was measured

- Repository, branch and initial/final SHA: `d-aicore/core`, worktree `D:\CORE\CORE\.agents\worktrees\maps-en`,
  branch `docs/maps-en`, base `ed4642e` (main). One commit carries the five maps, the plan update, this report
  and the index row; its SHA is in `git log` of that branch (the report is part of the commit, so it cannot
  quote its own SHA in advance).
- Scope and authorization: first task of the handoff plan (`docs/plan/README.md`, "The maps"): translate the
  five read-only Portuguese studies of commit `e7968c8` (in `D:\CORE\plan\`, outside the repo) into English in
  `docs/plan/`, preserving `path:line` references, verdict labels and the owner's verbatim quotes. Writing task
  only; no source code touched; the PT sources were not modified.
- Environment: Windows (win32, 10.0.26300), Git Bash, Node.js v24.22.1 used only for the preservation-count
  script. Main checkout `D:\CORE\CORE` left untouched (it has other sessions' uncommitted work in `ESTADO.md`
  and `docs/relatorios/`); all file work happened in the isolated worktree. Nothing pushed.
- Tests executed: **none**. This is a docs-only change; **no gate was run (`ci\gate.ps1`) because no code
  changed**. No product test, build or self-test was executed or is claimed.
- Limitations: translation fidelity was verified mechanically (counts and multisets, below) and by full
  reading, not by a second independent translator. The studies themselves were not re-audited: their findings,
  verdicts and measurements are carried over exactly as delivered, pinned to `e7968c8`.

## Results

| ID | Problem / result | Evidence (SHA, file:line or execution) | Classification | State |
| --- | --- | --- | --- | --- |
| MAPS-EN | Five English maps delivered in `docs/plan/`: `data-loss-map.md`, `plugin-hosting-map.md`, `realtime-map.md`, `dead-code-inventory.md`, `ux-pulse-spec.md` | Full translations of `perda-de-dados.pt.md`, `hosting-de-plugins.pt.md`, `tempo-real.pt.md`, `codigo-morto.pt.md`, `ux-pulse.pt.md` (studies of `e7968c8`); preservation counts measured by script, all equal (table below) | documentation; counts measured by script | delivered |
| MAPS-EN-PLAN | `docs/plan/README.md` "The maps" now says the English maps live in this folder and lists the five filenames; the e7968c8 pin note kept | worktree diff of `docs/plan/README.md`, section "The maps" only | documentation | delivered |
| MAPS-EN-IDX | One index row appended to `docs/relatorios/README.md` (committed version at `ed4642e`; no reformat, older rows untouched) | worktree diff of `docs/relatorios/README.md` | documentation | delivered |

### Preservation-count table (source vs target, measured by script)

Regex for references: `[A-Za-z_][A-Za-z0-9_./]*\.(cpp|h|cmake|ps1|json|md):\d+`; headings: lines starting
`#`…`######` + space; quotes: occurrences of `«`. Besides the counts, the script compared the **sorted
multisets** of the `path:line` references and of the full `«…»` spans: identical for all five pairs (so every
reference and every quoted span survived byte-for-byte, not just in number).

| Pair (PT source → EN target) | « src/tgt | path:line src/tgt | headings src/tgt | multisets | EN lines |
| --- | --- | --- | --- | --- | --- |
| perda-de-dados.pt.md → data-loss-map.md | 32 / 32 | 41 / 41 | 17 / 17 | identical | 185 |
| hosting-de-plugins.pt.md → plugin-hosting-map.md | 12 / 12 | 52 / 52 | 6 / 6 | identical | 184 |
| tempo-real.pt.md → realtime-map.md | 8 / 8 | 35 / 35 | 8 / 8 | identical | 185 |
| codigo-morto.pt.md → dead-code-inventory.md | 35 / 35 | 114 / 114 | 16 / 16 | identical | 185 |
| ux-pulse.pt.md → ux-pulse-spec.md | 114 / 114 | 9 / 9 | 18 / 18 | identical | 295 |

The +4 lines per file are the added header note and verdict legend (two blockquote lines plus spacing); no
heading was split or merged, no content removed.

### Verdict-label legend (1:1, none upgraded or downgraded)

- `data-loss-map.md`: confirmado por leitura → confirmed by reading; reproduzido → reproduced; confirmado e
  reproduzido → confirmed and reproduced; parcial → partial; refutado → refuted (refutado na prática → refuted
  in practice); por leitura → by reading; não verificado → not verified.
- `plugin-hosting-map.md`: **[lido] → [read]** (confirmed by reading); **[medido] → [measured]**;
  **[n/v] → [n/v]** (not verified). Confirmado/parcial prose → confirmed/partial.
- `realtime-map.md`: confirmado por leitura → confirmed by reading; confirmado por execução → confirmed by
  execution; medido → measured; não medido → not measured; não decidido → not decided; latente → latent;
  refutado → refuted; corrigido → fixed; confirmado → confirmed; suspeita → suspected; por leitura → by reading.
- `dead-code-inventory.md`: **CL** and **NV** markers kept as written; definitions translated (CL = confirmado
  por leitura = confirmed by reading; NV = não verificado = not verified).
- `ux-pulse-spec.md`: **[C]**, **[D]**, **[V]**, **[NV]** markers kept as written; definitions translated
  ([C] read in Pulse's code; [D] documented only, no corresponding code; [V] seen in the screenshot;
  [NV] not verified). Compound qualifiers translated around the kept marker ([NV em execução] → [NV at runtime],
  [NV se serve] → [NV whether it fits]).

Each file carries its own short legend in the header blockquote.

### Translation decisions (ambiguities resolved)

1. **All `«…»` spans kept verbatim**, a superset of the owner-quote rule: the studies use guillemets for owner
   quotes and owner-facing phrases («Piano que dá para ver tudo», «Channel rack expansível», the VST2 SDK
   question), but also for app UI strings (already English), code quotations, a ledger label («por verificar»)
   and a quotation of another document («o motor toca-os», from `docs/PLANO-CORE-PLUGINS.md:50`). Keeping every
   span byte-identical guarantees rule 3 and the count check; short English glosses were added **outside** the
   guillemets where a Portuguese span would otherwise be opaque (e.g. «Tentar de novo» (Try again)).
2. **Numbers kept in the original PT format** (decimal comma, space thousands separators: 0,24; 8 589 883;
   157 ms) per the "numbers unchanged" rule; prose around them translated.
3. The original delivery blockquote ("In Portuguese; to be translated into English before entering the repo")
   was translated and kept as historical provenance; the required header note (translated from <source>, study
   of e7968c8, line numbers drifted) was added above it.
4. The «Bem» cross-references in the data-loss verdict table stay verbatim; the referenced heading is translated
   "What I verified and is fine" and the references carry the gloss (fine).
5. «quero implementar vst2 tambem» does not occur in the five studies; it lives in `docs/plan/README.md` item 10,
   which was left intact (only "The maps" section changed).
6. The index row was appended to the **committed** `docs/relatorios/README.md` at `ed4642e`; the main checkout's
   uncommitted extra rows belong to another session and were not touched or carried over.

## Changes

All in the worktree, docs only:

- `docs/plan/data-loss-map.md` (new) — translation of `perda-de-dados.pt.md`.
- `docs/plan/plugin-hosting-map.md` (new) — translation of `hosting-de-plugins.pt.md`.
- `docs/plan/realtime-map.md` (new) — translation of `tempo-real.pt.md`.
- `docs/plan/dead-code-inventory.md` (new) — translation of `codigo-morto.pt.md`.
- `docs/plan/ux-pulse-spec.md` (new) — translation of `ux-pulse.pt.md`.
- `docs/plan/README.md` — "The maps" section rewritten to point at the five English files; rest of the plan intact.
- `docs/relatorios/2026-11-18-maps-english.md` (new) — this report.
- `docs/relatorios/README.md` — one appended index row; no other change.

No implementation, no proposal of code changes. `ESTADO.md` not edited (D72/D80); `docs/historico/` not touched;
the PT sources in `D:\CORE\plan\` not modified.

## Verification

Commands actually run (Git Bash), with results:

- `git -C D:/CORE/CORE worktree add "D:/CORE/CORE/.agents/worktrees/maps-en" -b docs/maps-en ed4642e` — exit 0.
- `node C:/Users/vk_do/AppData/Local/Temp/maps-en-check.mjs <src> <tgt>` (five pairs; script written for this
  task in `%TEMP%`, not a repo deliverable) — final run exit 0, `ALL CHECKS PASS`: counts equal and both
  multisets identical for every pair
  (table above). One earlier intermediate run exited 1 because the legend itself contained a `«…»` pair
  (33 vs 32 in the data-loss map); fixed by removing the guillemets from the legend before committing.
- Heading/reference/quote extraction used the regexes stated above; no re-anchoring of line numbers was done.
- **No build, no gate, no product test was run** — docs-only change; `ci\gate.ps1` deliberately not executed.

## Pending items and handoff

**PROPOSED lines for `ESTADO.md` (NOT written — D72/D80, only the ledger scribe writes them; in Portuguese to
match the ledger):**

- *Estado medido:* «Mapas em inglês (18/11, tarefa 1 do plano de handoff): os cinco estudos PT de `e7968c8`
  foram traduzidos na íntegra para `docs/plan/` (`data-loss-map.md`, `plugin-hosting-map.md`, `realtime-map.md`,
  `dead-code-inventory.md`, `ux-pulse-spec.md`), branch `docs/maps-en` a partir de `ed4642e`. Preservação medida
  por script: guillemets 32/12/8/35/114, `path:line` 41/52/35/114/9, cabeçalhos 17/6/8/16/18 — fonte = alvo, multiconjuntos
  de referências e de citações idênticos; citações do dono verbatim. Só documentação; nenhum código alterado;
  **gate não corrido por não haver código**. Relatório: `docs/relatorios/2026-11-18-maps-english.md`.»
- *Pendência (owner = Diogo / escrivão):* «Registar a entrada documental acima (sem decisão nova). As perguntas
  ao dono contidas nos mapas continuam por responder e não são ratificadas pela tradução: SDK VST2 (cópia
  legítima vs cabeçalho de raiz, hosting map §4c), código morto (5 dúvidas, inventory §Dúvidas), UX Pulse
  (4 perguntas, spec §10). A pendência das cinco citações do dono de 18/11 assinalada no ux-pulse §7 continua
  por abrir pelo escrivão.»

Next steps: the coordinator merges `docs/maps-en` into `main` when ready (this delivery does not merge or push);
the scribe records the proposed lines; subsequent plan tasks (items 1–11 of `docs/plan/README.md`) can now read
the maps in English. Acceptance suggestion for the Test Engineer: re-run the count script on the committed files
(any Node ≥ 18; regexes above) and spot-check that no verdict changed strength and that every owner quote is
byte-identical to the PT source. Residual risk: translation nuance only — the mechanical preservation is proven;
no runtime behavior is affected by this change.