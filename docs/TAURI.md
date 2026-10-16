# Tauri — overlay/UI (Etapa L em andamento)

> Status: **scaffold Svelte + TS + Vite + Tauri v2** (decisão travada).
> Stack: `npm` + `package-lock.json`, Node LTS pinado no `mise.toml`.
> Scaffold via `create-tauri-app --template svelte-ts`; prune do boilerplate
> no final. A UI só fala com o `core/` (ver `docs/CORE.md`) — nunca com o
> backend nativo diretamente.

## Layout (3 elementos)

```text
┌───────────────────┐
│ pid: 1234         │  <- topo, PID do processo injetado (editável em L)
│ ┌───────────────┐ │
│ │     60 FPS    │ │  <- meio, seletor do fps alvo (destaque);
│ └───────────────┘ │     fundo verde = limiter ON, transparente = OFF
│ 59.9 fps          │  <- base, fps atual, bem discreto (fps_avg, polling 1/s)
└───────────────────┘
```

* Sem slider: o seletor percorre os presets `30 40 45 60 72 90 120`
  (matriz validada na Etapa H). Sem item "livre" por agora; ON/OFF só via
  botão (não há valor livre no seletor).
* Desconectado (pipe ausente): topo mostra `—`, base mostra `—`, sem crash;
  o polling segue tentando.

## Controles

Teclado na Etapa L espelha o gamepad da Etapa M (mesmos comandos):

| Controle (M) | Teclado (L) | Ação |
|---|---|---|
| Cima/Baixo | `↑`/`↓` | troca o preset alvo |
| A (Xbox) / X (PlayStation) | `Enter`/`Espaço` | ON/OFF (área verde acende/apaga) |
| B | `Esc` | esconde o overlay |
| Start+Select | `F11` | mostra/esconde o overlay |

## Requisitos

* Janela sem borda, pequena (~260x220), overlay no canto; transparente quando
  apropriado (`decorations:false, transparent:true, alwaysOnTop:true,
  skipTaskbar:true, focus:false` — só na fatia L2; L1 é janela normal).
* **Nunca roubar foco do jogo** desnecessariamente.
* PID editável em L; detecção automática do jogo é Etapa N.
* Fronteira isolada: `src/lib/limiter.ts` (wrappers `invoke()`) → comandos
  Tauri (`set_target`, `set_enabled`, `get_stats`) → `qpslim-core` como
  dependência path → pipe `\\.\pipe\qpslim-<pid>`.
* Aviso anti-cheat **fora da UI**: vive na página de download do app no
  GitHub, não no overlay.

## Testes

* Frontend (`mise run app-test`, Vitest + `mockIPC`, roda no Linux): contrato
  IPC dos wrappers em `src/lib/limiter.ts` + helpers puros em
  `src/lib/selection.ts`. Sem teste de componente (custo/benefício ruim
  para 3 elementos).
* Comandos Rust (`src-tauri/src/lib.rs`, módulo `ipc_tests`, runtime mock):
  registro dos 3 comandos + desserialização dos args + mapeamento de erro
  para `String`. Execução real no Windows (`cargo test`); no Linux só
  compilação (`--target x86_64-pc-windows-gnu`). O caminho feliz exige pipe
  real e segue no roteiro manual contra o `inject-test`.

## Fatias

1. **L0 — scaffold:** template Svelte+TS + `qpslim-core` path + 3 comandos IPC.
2. **L1 — painel usável:** layout acima em janela normal + atalhos + polling.
3. **L2 — overlay real:** flags sem-foco/transparência/topmost + canto da tela.
