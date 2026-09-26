# Native — backend C++ (hook + frame pacer)

> Camada que vive **dentro do processo do jogo**. Idioma: C++.
> Build: `zig build` (ver `build.zig`). Tudo do código em inglês.

## O que há aqui

```text
native/
├── build.zig          # build de todos os alvos nativos
├── build.zig.zon      # manifesto do pacote
├── common/            # código compartilhado, livre de plataforma
│   ├── stats.h/.cpp   # matemática de medição (+ testes em stats_test.zig)
├── limiter/           # limiter.dll: DllMain + exports + hook de Present
├── graphics-api/
│   └── dx11/main.cpp  # harness de teste (Etapa A) — descartável no longo prazo
└── injector/          # (futuro) injector.exe: carrega a DLL no jogo
```

## Build

```bash
mise run build      # da raiz → native/zig-out/bin/
mise run release    # -Doptimize=ReleaseFast (medição real)
```

Direto: `cd native && zig build`. Alvo fixo `x86_64-windows-gnu`; saída
`zig-out/bin/`. Notas:

* System libs Win32 são listadas manualmente no `build.zig` (sem elas o link
  falha — o `zig build` não adiciona defaults como o CMake fazia).
* `exe.subsystem = .Windows` (GUI, sem console).
* Entry `WinMain` ANSI: o CRT mingw resolve `WinMain` sem flags extras; a
  linha de comando é ignorada e todo texto usa APIs `W`.
* O Zig linka `libc++`, não `libstdc++` — ok para o PoC; reavaliar se o hook
  exigir ABI específica.
* Gera `.pdb` junto — útil com PIX/WinDbg.

## dx11-test (Etapa A)

Janela Win32 + device/swapchain D3D11, `Present(0,0)` sem VSYNC, cor animada e
FPS no título via `QueryPerformanceCounter`. Fallback WARP se não houver GPU.

Execução real só no Windows 10/11: levar `native/zig-out/bin/dx11-test.exe`.
Esperado: janela 1280x720 pulsante, `FPS: NNN` no título, resize ok, ESC fecha.
Logs saem via `OutputDebugString` (DebugView) — base para comparar contagem
interna vs. contagem do hook (Etapas C/D).

## limiter.dll (Etapa E)

Esqueleto + hook + **cap ingênuo**: `Limiter_SetTargetFps(int)` (`0` = livre)
arma `intervalo = freq/fps`; cada `Present` dorme o restante até o deadline
(`Sleep`, ms truncados) e o deadline sempre avança um intervalo — sem
rajada de catch-up. Sem correção de drift, sem spin: pacing bom é a Etapa G.
O teste fixa 60 FPS via `kTargetFps` após o hook.

O hook (`hook.h`/`hook.cpp`) troca o slot 8 da vtable da swapchain pelo nosso
`HookedPresent`, que conta (atômico), repassa ao original e volta. Log no
DebugView a cada 300 presents. `UnhookSwapChain()` restaura o slot — chamado
pelo teste na saída, antes de liberar a swapchain. O `DLL_PROCESS_DETACH`
só loga: patch sob loader lock arrisca deadlock, então a reversão é sempre
explícita pelo host.

Limitação conhecida (Stage C): um patch cobre todas as swapchains da mesma
classe COM, mas outra classe (ex.: segunda janela via `IDXGISwapChain1`) tem
vtable própria e fica de fora. Cobertura multi-classe vem com a descoberta
automática, fora desta etapa.

## Medição (Etapa D)

Cada `Present` carimba `QueryPerformanceCounter`; deltas em ticks vão para um
ring buffer de 240 amostras (sem alocação/lock no caminho quente — só inteiros).
`Limiter_GetStats()` devolve `{ present_count, fps_avg, frametime_avg_ms,
frametime_max_ms }` sobre a janela; o teste lê a cada 2s e cruza com o
próprio contador. Single-writer, leitor ocasional, diagnóstico.

## Testes

`mise run test` (da raiz) roda `common/stats_test.zig` no host via
`zig build test`: matemática pura de `common/stats.h` (`WindowBounds`,
`StatsCompute`) — sem Windows, sem D3D. Hook, DLL e teardown continuam
manuais no Windows. Logs periódicos do teste saem a cada 2s para não
inundar o DebugView.

Detalhe de build: a DLL linka `kernel32` + libc mingw (headers + startup),
**sem** `libc++` — usa só Win32 API + `<stdio.h>`, de propósito. O `dx11-test`
carrega via `LoadLibrary("limiter.dll")` e entrega sua swapchain via
`Limiter_HookSwapChain`; sem a DLL, roda normal (carga opcional).

## Contrato futuro (a partir da Etapa C)

* `DllMain` mínimo: só log + `DisableThreadLibraryCalls` (loader lock).
* API: `enable()` / `disable()` / `set_target_fps(n)` — a UI nunca fala com o
  backend diretamente (ver `docs/CORE.md`).
* Sem dependência da Steam; alvo de teste exclusivo é o `dx11-test` até o
  pacing estar provado. Sem anti-cheat/DRM — ver `docs/PLAN.md` §9.
