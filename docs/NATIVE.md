# Native — backend C++ (hook + frame pacer)

> Camada que vive **dentro do processo do jogo**. Idioma: C++.
> Build: `zig build` (ver `build.zig`). Tudo do código em inglês.

## O que há aqui

```text
native/
├── build.zig          # build de todos os alvos nativos
├── build.zig.zon      # manifesto do pacote
├── dx11-test/         # alvo de teste do PoC (Etapa A) — descartável no longo prazo
│   └── main.cpp
├── limiter/           # limiter.dll: DllMain + exports (hook de Present a partir da C)
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

## limiter.dll (Etapa B)

Esqueleto carregável: `DllMain` mínimo (só log + `DisableThreadLibraryCalls`)
e um export `Limiter_GetVersion()`. Sem hook ainda.

Detalhe de build: a DLL linka `kernel32` + libc mingw (headers + startup),
**sem** `libc++` — usa só Win32 API, de propósito. O `dx11-test` carrega via
`LoadLibrary("limiter.dll")` se existir ao lado do exe; sem a DLL, roda
normal (carga opcional).

## Contrato futuro (a partir da Etapa C)

* `DllMain` mínimo: só log + `DisableThreadLibraryCalls` (loader lock).
* API: `enable()` / `disable()` / `set_target_fps(n)` — a UI nunca fala com o
  backend diretamente (ver `docs/CORE.md`).
* Sem dependência da Steam; alvo de teste exclusivo é o `dx11-test` até o
  pacing estar provado. Sem anti-cheat/DRM — ver `docs/PLAN.md` §9.
