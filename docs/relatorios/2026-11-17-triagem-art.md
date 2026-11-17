# Relatório — 2026-11-17 — Triagem do lote A-real-time (ids ART-)

> Arquivo verbatim da entrega de um subagente de triagem. Este cabeçalho é do coordenador; o corpo, a partir de
> «# Triagem ART», está tal como foi entregue e não foi editado.

## Proveniência

- **Quem:** subagente `triagem-art` (sessão filha DSH `86671b31-8f18-4756-95bf-f99192578e98`) da sessão do coordenador
  `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`. Modelo medido no log da sessão filha: `anthropic/claude-fable-5-2` (116 mensagens).
- **Quando:** 17/11/2026, entregue às 22:17 (Europe/Lisbon).
- **Árvore:** `7211c73`. O lote foi fixado pelo auditor em `8850edd`; `git diff 8850edd 7211c73 -- engine/` vazio.
- **Método:** só leitura. Nada compilado, executado nem escrito pelo subagente.
- **Entrada:** lote do auditor externo, 18 achados, recebido na sessão DSH `session-f917643e-3439-450a-856b-db4965395354`
  (seq 475). A-001 e A-009 já estavam refutados; foram triados os 16 restantes. Ids renomeados `A-0NN` → `ART-0NN`
  para não colidirem com o lote A anterior.

## Notas do coordenador (17/11 22:35)

- **Contagem:** a frase de abertura diz «3 confirmados, 7 parciais, 4 refutados, 2 não decidíveis»; a tabela do §2 dá
  5 confirmados / 6 parciais / 3 refutados / 2 não decidíveis. Vale a tabela.
- **FL-1** foi conferido por mim no código (`server.cpp:100`, `workers.hpp:37-40`, `ESTADO.md:39`; 0 ocorrências de
  `liveThreads` em `app/` e em `tests/`) e **corrigido em `e436ede`** — ver
  [FL-1 — M5 default](2026-11-17-fl1-m5-default.md). As linhas de `server.cpp` citadas no corpo são as de `7211c73`.
- **Estatuto:** todo o resto é «confirmado no código por leitura», por um único verificador, e **não foi conferido por
  mim linha a linha**. Nada aqui é «comprovado por teste» excepto o FL-1.

## Linhas propostas para o ESTADO.md (não escritas)

- **Estado medido:** desfecho da triagem por achado (tabela do §2), com o estatuto acima.
- **Invalidadas por referência:** ART-006, ART-013 e ART-014 enquanto defeitos em aberto (fecham sem acção; ART-014
  reabre se alguém passar a chamar `setNativeProtection(false)`).
- **Pendências:** ART-003 (notificação de dispositivo), ART-018 (áudio partido ao carregar), FL-2 (a app nunca envia
  `plugin.retry`; aviso de `MainComponent.cpp:6345`), ART-002 (política do bus em falha), FL-3, os baixos
  ART-004/008/010(b)/011/012/015/016/017, e ART-005 e ART-007 (só se decidem medindo).
- **Numeração:** `ART-0NN` substitui `A-0NN` deste lote em todas as referências futuras.

---

# Triagem ART (lote A-real-time) — HEAD 7211c73

O achado mais importante não é nenhum dos 16: **o M5 está ligado por omissão no caminho do produto**, ao contrário da D51 e da premissa do auditor. Isso muda o alcance de ART-010. Dos 16: 3 confirmados, 7 parciais, 4 refutados (um só quanto ao alcance) e 2 não decidíveis por leitura. Nada foi executado, compilado nem escrito.

## 1. Cabeçalho

- **SHA medido:** `7211c731cddca2cf860a2fc63dcccfdf70765335` (main). `8850edd` é ancestral. Árvore de trabalho: só `.qoderignore` por seguir.
- **Diff 8850edd..HEAD:** `-- engine/` vazio. O diff total só toca `ESTADO.md`, `ci/compilar.bat`, `docs/relatorios/*` e `tests/engine_tests.cpp` (+37). `app/` e o `CMakeLists.txt` da raiz não mudaram, logo todas as linhas citadas valem.
- **Lidos por inteiro:**
  - `engine/src/core`: graph.cpp, graph.hpp, engine.cpp, engine.hpp, project.cpp, server.cpp, server.hpp, workers.hpp, rt_workers.hpp, plugins.hpp, tempo_map.cpp, assets.cpp
  - `engine/src/io`: device.cpp, audio_file.cpp
  - `engine/src/dsp`: output_guard.cpp/.hpp, finite.hpp, float_mode.hpp, loudness.cpp/.hpp, analyzer.cpp, effects.cpp/.hpp, instruments.cpp/.hpp, registry.cpp
  - `engine/src/plugins/juce_host.cpp`, `engine/src/app/main.cpp`, `engine/src/app/measure.cpp`, `engine/CMakeLists.txt`, `CMakeLists.txt`
- **Lidos em parte:**
  - project.hpp (1-70), render_job.cpp (155-200, 230-399)
  - `app/desktop/src/EngineSession.cpp` (310-404, 455-544), `app/desktop/src/MainComponent.cpp` (6330-6351)
  - `app/engine-client/src/engine_client.cpp` (60-119)
  - `app/domain/src/document.cpp` (514-554, 624-637), `app/domain/src/edit.cpp` (296-309)
  - `miniaudio.h` v0.11.22 (20925-21004, 21040-21124, 22958-22979, 23056-23130, 41010-41084, 41296-41329), `json.hpp` (12438-12451)
  - `tests/` e `ESTADO.md` só por grep
- **JUCE não está vendored no repo.** Está fixado em `CMakeLists.txt:65` «URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz  # commit d60b7a78030661574a615b586d9325ba7a7fa7dd». Li a cópia local `D:\Core-deps\JUCE-9.0.5` (a que `ci/gate.ps1:33` usa); não verifiquei o hash dela.

**Pressupostos de configuração (passo 4):**

- **/fp:fast: sim.** `engine/CMakeLists.txt:9` «  add_compile_options(/W3 /utf-8 /fp:fast)». Vale para todos os alvos de `engine/` em MSVC.
- **M5 não está desligado por omissão no produto.**
  - Só o construtor tem 0: `engine.hpp:76` «  explicit Engine(double sampleRate, int liveThreads = 0)».
  - O servidor usa `server.cpp:100` «  if (v.empty()) return WorkerPool::forThisMachine(4);» e `server.cpp:325` «    const int live = (int)std::clamp(numArg(args, "liveThreads", (double)liveThreadsFromEnv()), 0.0, 32.0);».
  - A app não envia `liveThreads` (`EngineSession.cpp:507-509` só põe backend, device e listenDb) e lança o motor com o ambiente herdado (`engine_client.cpp:94`, lpEnvironment `nullptr`).
  - Medido nesta máquina, só leitura: 16 processadores lógicos, `CORE_LIVE_THREADS` vazio em Process/User/Machine. Resultado esperado: 4 workers.
- **`setNativeProtection(false)`: inalcançável.** `git grep` em todo o repo dá só a definição (`graph.hpp:186`) e o membro (`graph.hpp:370`). Nem testes o chamam.
- **JUCE 9.0.5, limite real de 32 canais:** `juce_AudioSampleBuffer.h:1214` «        if (numChannels < (int) numElementsInArray (preallocatedChannelSpace))» e `:1268` «    Type* preallocatedChannelSpace[32];».
- **Os 4 sítios do Graph recebem Project validado:**
  - `server.cpp:202`: `project_` só muda por `fromJson` (:449), `trackFromJson` via `buildTrack` (:480, :516), BPM (:470) e ganho master (:525).
  - `server.cpp:113`: cópia de `project_` (:656) ou `fromJson(demoProject)` em measure.cpp.
  - `render_job.cpp:290`: cópia de `project_` (`server.cpp:607`).
  - `measure.cpp:221`: «  for (int i = 0; i < 100; ++i) Graph g(fromJson(demoProject(500)), 48000);».

## 2. Tabela resumo

| id | veredicto | gravidade real | alcançável por omissão? | correcção toca graph.cpp/engine.cpp? |
|---|---|---|---|---|
| ART-002 | CONFIRMADO (política M3b, decisão de produto) | médio | sim | sim (graph.cpp) |
| ART-003 | CONFIRMADO no código; runtime por medir | médio | sim | não |
| ART-004 | PARCIAL | baixo | corte audível: sim; intervalo: não | não |
| ART-005 | NÃO DECIDÍVEL POR LEITURA | não medido (estimo baixo) | sim (o caminho corre) | sim, se medir mal |
| ART-006 | REFUTADO | nulo | não | — |
| ART-007 | NÃO DECIDÍVEL POR LEITURA (dobra) | baixo | sim | sim (2 linhas) |
| ART-008 | CONFIRMADO por leitura; probabilidade baixa | baixo | sim | sim (engine.cpp) |
| ART-010 | PARCIAL (premissa «M5 desligado» falsa) | baixo | **sim** | por omissão: não (server.cpp); (b): sim |
| ART-011 | PARCIAL (mecanismo sim, alcance por medir) | baixo | só com plugin ≥32 canais | não |
| ART-012 | PARCIAL | baixo | só se um plugin mexer no MXCSR | não |
| ART-013 | REFUTADO (alcance) | nulo | não | não |
| ART-014 | REFUTADO (alcance) | nulo | não | — |
| ART-015 | CONFIRMADO como latente | baixo | não | não |
| ART-016 | PARCIAL | baixo (nulo pela app) | não pela app; sim por CLI | não |
| ART-017 | PARCIAL | baixo | sim | não |
| ART-018 | CONFIRMADO (uma parte cai) | baixo | sim, com ficheiro partido | não |

## 3. Por achado

### ART-002 — CONFIRMADO (comportamento documentado)
- **Prova:** `graph.cpp:303` «        auto e = std::make_unique<PluginEffect>(sampleRate, std::move(processor), owner == "master");». O bus entra com o seu id: `graph.cpp:361` «    rt.anyEffect = buildChain(t.effects, rt.effects, t.id);». Em falha: `graph.cpp:246` «    std::fill(left, left + frames, 0.0f);  // on a track: the track is silent from here on».
- **É política escrita:** `server.cpp:54` «…each is silent (its track mutes; on the master the mix goes through) until plugin.retry…».
- **Cai** a «incoerência com os efeitos nativos»: já não existem. `effects.cpp:10` «std::unique_ptr<Effect> makeEffect(const std::string&, double, double, int) { return nullptr; }».
- **Alcance:** qualquer bus com insert de plugin que lance, crashe ou dê NaN.
- **Impacto:** o bus cala tudo o que lhe chega, e fica calado (ver FL-2: a app não tem Retry).
- **Correcção:** decisão do dono: bus passa o sinal seco, como o master? Teste: 3 faixas para um bus com processador falso que lança; a saída master não pode ser zero depois da falha.

### ART-003 — CONFIRMADO no código
- **Prova:** device.cpp só configura `:131` «    config.dataCallback = callback;» e `:132` «    config.pUserData = this;». Não há `notificationCallback` nem consulta de estado no ficheiro inteiro.
- **miniaudio, dispositivo por omissão:** reencaminha sozinho. `miniaudio.h:22970` «            pDevice->wasapi.allowPlaybackAutoStreamRouting = MA_TRUE;», só com `pDeviceID == NULL` (:22969). Depois reamostra do ritmo do motor para o novo: `miniaudio.h:41017` «    if (pDevice->sampleRate == 0) {» e `:41065` «        converterConfig.sampleRateOut                   = pDevice->playback.internalSampleRate;».
- **miniaudio, dispositivo escolhido por nome:** `miniaudio.h:22961-22962` «…Otherwise we want to just / stop the device outright and let the application handle it.».
- **Alcance:** por omissão. `EngineSession.cpp:508` «    if (!options_.device.empty()) args["device"] = options_.device;».
- **Impacto:** interface desligada → sem callbacks, `playing` verdadeiro, posição parada, nenhum erro. Mudança do dispositivo por omissão → reamostragem silenciosa e `deviceBufferNs_` desactualizado (só é posto em `device.cpp:162`).
- **Correcção:** `notificationCallback` → flags atómicas → evento de estado → a app reabre. Teste: manual (desligar USB; mudar o dispositivo por omissão para 44,1 kHz).
- **Por verificar:** o que o ciclo WASAPI faz quando o dispositivo explícito é invalidado (não li); suspensão e retoma do PC.

### ART-004 — PARCIAL (concordo com a pista, com dois acrescentos)
- **Cai o UB do sampleRate:** `server.cpp:323` «    if (!(sr >= 8000 && sr <= 384000)) throw std::runtime_error("sampleRate must be 8000-384000.");» vem antes de `server.cpp:337` «    if (kept) kept->stop();». `numArg` recusa NaN/Inf (`server.cpp:75-76`).
- **O `stop()` de `device.cpp:116` é inócuo:** cada arranque usa um objecto novo, `server.cpp:349` «      output_ = makeOutput_();».
- **A reposição existe:** `server.cpp:378` «          outputInfo_ = output_->start(*engine_, keptArgs);  // as it was started before this command».
- **Sobrevive (i):** backend, device, blockSize e periods só falham depois de :337. Um arranque recusado dá um corte audível (parar e reabrir), não perda.
- **Sobrevive (ii):** `device.cpp:126` «    config.periodSizeInFrames = (ma_uint32)block;» e `:127` «    config.periods = (ma_uint32)args.value("periods", 2);» sem intervalo.
- **Alcance de (ii):** a app nunca os envia; só a CLI `--block` (`main.cpp:333`, `:454`).
- **Correcção:** validar intervalos em server.cpp antes de :337. Teste: estender `engine_tests.cpp:452` com `{blockSize:-1}` → erro e a saída anterior continua arrancada.
- **Por verificar:** o que o miniaudio faz com um período enorme (não li).

### ART-005 — NÃO DECIDÍVEL POR LEITURA
- **O caminho corre em cada edição a tocar:** `engine.cpp:228` «      next->adopt(*current_);» e `:230`. A app manda cada edição como `project.load` inteiro (`EngineSession.cpp:539`).
- **Limite por leitura:** `forEachHeld` percorre todos os eventos do clip (`graph.cpp:884` «      for (const SeqEvent& e : clip.events) {»), mas no máximo ~3 repetições, porque `project.cpp:404` «    if (n.length < 1 || n.length > patternTicks) throw std::runtime_error("invalid note length.");».
- **Custo por conversão em tempo constante:** uma multiplicação e um `llround` (`tempo_map.cpp:44`, `:48`).
- **adopt dos efeitos é vazio:** `graph.cpp:249` «  void adopt(Effect&) override {}  // the instance itself goes on playing». Sem alocações nem locks.
- **Estimativa não medida:** 30 faixas × 64 notas ≈ 10⁴ conversões, fracção de ms. Só padrões de milhares de notas em dezenas de faixas com buffer de 128 chegam ao orçamento.
- **Medição em falta:** `core-engine soak <canção mais pesada> --block 128` (já edita de 10 em 10 s, `main.cpp:485-491`) e ler `overruns`, `maxLoadPct`, `maxRenderMs`.

### ART-006 — REFUTADO (concordo com a pista)
- `project.cpp:552` «  if (t.patternTicks < kPpq / 8 || t.patternTicks > 64LL * 4 * kPpq) throw std::runtime_error("invalid pattern length.");».
- `project.cpp:570` «        if (c.patternTicks < kPpq / 8 || c.patternTicks > 64LL * 4 * kPpq) throw std::runtime_error("invalid pattern length.");». Com kPpq = 960, o mínimo é 120.
- «Notas longas → milhares de repetições» cai por `project.cpp:404` (acima).
- **Resíduo nulo:** o ciclo de `sourceOns` (`graph.cpp:479`) só cresce com um projecto escrito à mão para isso, e corre no thread de controlo.
- **Não verifiquei** se já há teste de recusa para 0, 1, 119 e -960.

### ART-007 — NÃO DECIDÍVEL POR LEITURA (a dobra), impacto limitado
- **Prova:** `graph.cpp:120` «    if (std::isfinite(l) && std::isfinite(r) && std::fabs(l) <= kRunawayLevel && std::fabs(r) <= kRunawayLevel) continue;», contra a regra de `finite.hpp:2-3`.
- **Mesmo dobrado, o NaN não chega às colunas:**
  - segunda rede com `finiteBits`, sempre ligada: `graph.cpp:1283` «      if (protectNative_ && nativeOutputBad(buf, bufR, n)) {» (e `:1232`, `:1097`);
  - terceira: `output_guard.cpp:49`.
- **O que muda:** o rótulo e a política. A falha conta como «unit», e `server.cpp:434` «    if (!it->processor->failed()) return {{"retried", false}};» deixa o retry sem efeito.
- **Medição:** o teste do auditor em Release MSVC, ou o código gerado de `checkOutput`.
- **Correcção:** `finiteBits` em graph.cpp:120-121. Teste: processador falso com NaN → `failure()==NonFinite`, `pluginFailures()` sobe, `nativeFailures()` não.

### ART-008 — CONFIRMADO por leitura
- **Controlo:** `engine.cpp:108` «  loopStart_.store(start);» … `:111` «  loopForPending_.store(pending_.load(std::memory_order_acquire) != nullptr, std::memory_order_release);».
- **Áudio:** `engine.cpp:209` «      const bool loopAlreadyNew = loopForPending_.exchange(false, std::memory_order_acq_rel);  // set for this tempo: stays» e `:215` «            loopStart_.store((int64_t)std::llround(loopStart_.load() * ratio));». Nada coordena os dois lados.
- **Terceira janela, minha, mesmo mecanismo:** flag posta com um grafo pendente do mesmo tempo; um `publish` com tempo novo substitui-o antes de haver callback; a troca salta a conversão.
- **Alcance:** `transport.loop` (`EngineSession.cpp:326`) junto a uma edição de tempo. Janelas de nanossegundos, ou de um período de callback na terceira.
- **Impacto:** região de loop errada até ao próximo ajuste. Sem crash (`engine.cpp:335` «      if (end > start) {») e sem dados perdidos.
- **Correcção:** publicar o loop como uma unidade marcada com a geração do tempo. Teste: o stress do auditor, mais um determinista para a terceira janela.

### ART-010 — PARCIAL: factos certos, âmbito errado
A premissa «só M5, desligado por omissão» é falsa (ver cabeçalho e FL-1).
- **(a)** `graph.cpp:1421` «  if (live_->failed()) throw std::runtime_error("A track failed on a worker thread.");». Não encontrei fonte de excepção: as dos plugins morrem em `graph.cpp:103` «  } catch (...) {».
- **(b) Corrida real, em configuração por omissão.** `engine.cpp:349` «      timedOffs(frames - done);» chama `g.liveNote` (`:314`) enquanto um worker atrasado ainda pode estar em `PluginInstrument::renderStereo` (`graph.cpp:192` «    count_ = 0;»).
  - Exige um worker passar o prazo de 2 buffers (`engine.hpp:226` «  double liveDeadline_ = 2.0;») e uma audição a expirar no mesmo bloco.
  - Acontece no máximo uma vez por Engine: depois toca em série (`engine.cpp:196`).
- **(c)** `engine.cpp:50`, espera sem limite: só com um plugin preso para sempre.
- **(d)** `graph.cpp:46`, `snprintf`: só em falha.
- **Impacto de (b):** uma nota de audição pode ficar presa até Stop ou panic. Improvável.
- **Correcção:** primeiro o valor por omissão em server.cpp (FL-1). (b) só interessa se o M5 voltar a ligar.

### ART-011 — PARCIAL
- **Mecanismo confirmado na cópia local do JUCE 9.0.5:** `juce_AudioSampleBuffer.h:1220` «            allocatedData.malloc (numChannels + 1, sizeof (Type*));» quando há 32 canais ou mais.
- **No Core:** `juce_host.cpp:89` «    const int channels = std::max({2, instance_->getTotalNumInputChannels(), instance_->getTotalNumOutputChannels()});» e `:167` cria a vista em cada bloco de 128.
- **Alcance:** só plugins com 32 ou mais canais activos (multi-saída).
- **Medição:** correr `core-engine scan` sobre a biblioteca do dono; já devolve «outputs» (`juce_host.cpp:534`).
- **Correcção:** vista persistente criada uma vez, só em juce_host.cpp. Teste: plugin de teste com 32 saídas e contador de alocações.

### ART-012 — PARCIAL
- **Facto confirmado:** o modo é posto uma vez por callback, `device.cpp:58` «    setAudioFloatMode();  // FTZ + DAZ, whatever the backend did (float_mode.hpp); the workers take it from here». `graph.cpp:102` «    call();» não guarda nem repõe.
- **Cai o custo de denormais «nos efeitos nativos»:** não sobra DSP nativo (`effects.cpp:10`; `instruments.cpp:19` «  void render(float*, int) override {}»).
- **A fuga dura no máximo um callback** (`device.cpp:58`; workers: `rt_workers.hpp:144` «      setFloatMode(mode_);»).
- **Sobrevive como hipótese:** um plugin que desmascare excepções de vírgula flutuante e provoque SEH fora do `guardedCall`. Depende de plugins concretos.
- **Não verifiquei** as flags de excepção do MSVC (/EHsc).
- **Correcção:** guardar e repor o MXCSR à volta de `processBlock` em juce_host.cpp. Teste: plugin de teste com `_mm_setcsr(0x1F80)`.

### ART-013 — REFUTADO quanto ao alcance
- Todos os chamadores dão valores finitos: `server.cpp:393` «    listenDb_ = std::clamp(args["db"].get<double>(), OutputGuard::kListenMinDb, OutputGuard::kListenMaxDb);» depois de `is_number` (:392); o mesmo em `:341`.
- Os restantes: `server.cpp:137`, `:346`, `:351`, `:361` passam `listenDb_`; `main.cpp:193` e `:373` passam 0.
- O texto JSON não tem gramática para NaN.
- **Opcional:** `finiteBits` em `output_guard.cpp:23` (uma linha).

### ART-014 — REFUTADO quanto ao alcance
- **A ordem é a descrita:** `engine.cpp:397` «  loudness_.process(L, R, frames);» antes de `:400`.
- **O envenenamento seria real:** estado recursivo em `loudness.hpp:44` «      z1 = b1 * x - a1 * y + z2;» e `analyzer.cpp:51` «    ll_ = ll_ * decay_ + a * a;», sem reset.
- **Mas o NaN não chega lá:** todas as saídas de unidade passam por `nativeOutputBad` (`graph.cpp:1232`, `:1247`, `:1283`, `:1097`), e a protecção nunca é desligada.
- **Reabrir** só se alguém passar a chamar `setNativeProtection(false)`.

### ART-015 — CONFIRMADO como fragilidade latente
- **Prova:** `workers.hpp:88` «      (*job_)(i);» sem `try`. O pool de tempo real tem (`rt_workers.hpp:150` «      } catch (...) {»).
- **Sem fonte de excepção alcançável:** o job é `runTrack` (`graph.cpp:1512-1516`); as excepções dos plugins morrem em `graph.cpp:103`, os crashes em `juce_host.cpp:65-66`, e não vi alocações nem `at()` no percurso (1136-1337).
- **Impacto se acontecer:** `std::terminate` do motor durante uma exportação.
- **Correcção:** `try`/`catch` com `exception_ptr`, relançar depois da espera, só em workers.hpp. Teste: o do auditor.
- **Não verifiquei** se a app relança o motor.

### ART-016 — PARCIAL (discordo de «confirmado» sem mais)
- **Motor:** a colisão existe.
  - `project.cpp:514` «  static const std::regex idPattern("^[A-Za-z0-9_-]{1," + std::to_string(kMaxTrackIdLength) + "}$");» aceita «master».
  - `graph.cpp:333` «  anyMasterEffect_ = buildChain(p.masterEffects, masterEffects_, "master");».
  - Com a mesma chave e identidade o host devolve a mesma instância: `juce_host.cpp:306` «    if (it != impl_->live.end() && it->second.identity == identity && it->second.sampleRate == sampleRate) return it->second.processor;».
- **A app impede-a:**
  - `document.cpp:632` «  renameMasterTrack(d);» (definida em :522);
  - `edit.cpp:305` «  for (int n = 2; doc.findTrack(id) || id == "master"; ++n) id = root + "-" + std::to_string(n);»;
  - há teste em `tests/audit_tests.cpp:84` «  CHECK(track != "master");» (não corrido por mim).
- **Alcance real:** só por CLI (`core-engine render`/`soak`/`loopcheck`) ou outro cliente do protocolo.
- **Efeito sem id:** `graph.cpp:301` usa `std::to_string(chain.size())`, que pode coincidir com um id explícito «0» ou «1». `project.cpp:464` só deduplica ids não vazios. Não verifiquei se a app dá sempre id.
- **Impacto se atingido:** a mesma instância processada duas vezes por sub-bloco; ou, com plugins diferentes, reinstanciação a cada edição e um só estado guardado por chave.
- **Correcção:** só project.cpp (recusar «master»; dar id único a efeitos de plugin sem id). Teste: `fromJson` com faixa «master» → recusa.

### ART-017 — PARCIAL («menor»: concordo)
- **Confirmado:** `device.cpp:146` «          if (std::strstr(playback[i].name, args["device"].get<std::string>().c_str())) { id = playback[i].id; haveId = true; break; }».
- **Cai «backend só na primeira chamada»:** objecto novo em cada arranque (`server.cpp:349`) e em cada listagem, `server.cpp:318` «  if (cmd == "devices.list") { auto out = makeOutput_(); return out ? out->devices(args) : json::array(); }».
- **Cai `device=''`:** a app só envia nomes não vazios (`EngineSession.cpp:508`).
- **Impacto:** com dois nomes em que um contém o outro, pode abrir o errado. Vê-se na mensagem «Output: …» (`EngineSession.cpp:518`).
- **Correcção:** igualdade exacta primeiro, subcadeia depois (device.cpp).

### ART-018 — CONFIRMADO (uma parte cai)
- **Prova:** `graph.cpp:1247` «    if (protectNative_ && nativeOutputBad(buf, bufR, n)) { std::fill(buf, buf + n, 0.0f); std::fill(bufR, bufR + n, 0.0f); }», sem contador.
- **O float entra cru:** `audio_file.cpp:139` «        if (h.format == 3 && h.bits == 32) std::memcpy(&v, p, 4);». O `AssetStore` não verifica.
- **A exportação não dá por nada:** usa o mesmo grafo (`render_job.cpp:290`) e o seu teste de NaN (`render_job.cpp:179`) já só vê o silêncio.
- **Cai** «o instrumento de plugin da mesma faixa também é calado»: `project.cpp:580` «    if (t.instrument != "audio") throw std::runtime_error("audioClips belong to an audio track.");».
- **Alcance:** WAV float com NaN/Inf ou amostras × ganho acima de 256 (ganho do clip até +24 dB, `project.cpp:592`).
- **Impacto:** buracos de 32 amostras, ao vivo e no ficheiro exportado, sem aviso. Raro.
- **Correcção:** verificar ao carregar em `AssetStore::get` e reportar como áudio em falta. A exportação já aborta nesse caso: `render_job.cpp:298` «        throw std::runtime_error("Missing audio, nothing exported: " + list);». Teste: WAV float com 100 amostras a 1000 → `missingAssets` e `render.start` falha.
- **Por verificar:** se a importação da app saneia.

## 4. Ordem de ataque

**Não tocam graph.cpp/engine.cpp** (por risco ÷ custo):
1. **FL-1 / ART-010** — valor por omissão de `liveThreads` em server.cpp, para cumprir a D51. Tira (a)(b)(c) do alcance. Pode subir a carga em canções pesadas, por isso precisa de autorização explícita.
2. **ART-003** — notificação do dispositivo. É a falha mais provável no dia-a-dia.
3. **ART-018** — recusar áudio partido ao carregar. Protege a exportação.
4. **ART-016** — reservar «master» e ids únicos no motor. Barato.
5. **ART-004 + FL-3** — intervalos e o buraco da reposição, em server.cpp.
6. **ART-011 e ART-012** — vista persistente e MXCSR, em juce_host.cpp.
7. **ART-015** — `try`/`catch` nos workers.
8. **ART-017** — igualdade exacta.

**Tocam o caminho de áudio:**
1. **ART-007** — duas linhas, sem efeito em áudio finito.
2. **ART-002** — só depois de decisão de produto, e com FL-2.
3. **ART-005** — medir primeiro com o soak; corrigir só se medir mal.
4. **ART-008** — só com o teste de stress escrito antes.
5. **ART-010(b)** — só se o M5 voltar a ligar.

**Fechar sem acção:**
- **ART-006** — a validação existe.
- **ART-013** — nenhum chamador produz NaN.
- **ART-014** — inalcançável enquanto ninguém desligar a protecção nativa.

## 5. Fora do lote e não verificado

**Fora do lote:**
- **FL-1 (alto) — o código contradiz a D51.** `ESTADO.md:39` «M5 OFF por defeito. Só passa a ON por nova decisão minha…». O servidor dá 4 workers (`server.cpp:100`, `:325`), o comentário de `engine.hpp:74` diz o contrário, e a app não passa `liveThreads`. Confirmar em execução com o campo «liveThreads» de `engine.stats` (`server.cpp:421`).
- **FL-2 (médio) — plugin falhado fica falhado.** A app nunca envia `plugin.retry` (em `app/` só existe `EngineSession.cpp:385` «            client_->command("plugins.release");»), e o host devolve a mesma instância em cada reconstrução (`juce_host.cpp:306`). O aviso de `MainComponent.cpp:6345` «…silent until your next change: » é falso para plugins. Não verifiquei se alguma edição muda o `stateId`.
- **FL-3 (baixo) — buraco na reposição de `engine.start`.** Só há motor guardado quando `otherEngine` (`server.cpp:338` «    if (otherEngine) keptEngine = std::move(engine_);»). Se o dispositivo escolher outra taxa (`:359` substitui o motor) e o segundo arranque falhar, `:373` «      sampleRate_ = keptRate;» fica desalinhado com o motor.

**Não verificado:**
- Nada foi executado: nem testes, nem gate, nem a app.
- Não li: `tempo_map.hpp`, `registry.hpp`, `clip_grains.hpp`, `analyzer.hpp`, `true_peak.hpp`, `wav.cpp`, `qualify.cpp`, `render_job.hpp`, `juce_host.hpp`.
- Do miniaudio e do JUCE só li os troços listados.
- Da app só li os troços listados. Em particular, não sei se ela detecta um dispositivo parado.
- A triagem é só leitura: não escrevi o relatório em `docs/relatorios` que o `AGENTS.md` pede, nem memória. FL-1 merece pendência no ledger; fica para o escrivão.
