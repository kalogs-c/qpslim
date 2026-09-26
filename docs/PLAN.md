# FPS Limiter para Windows — Plano do Projeto

> Documento vivo de contexto. **Atualizar `Status` e `Roadmap` ao concluir cada etapa.**
> Idioma do projeto: português (docs). Código: inglês. Alvo final: Windows 10/11.

## Status

| Campo | Valor |
|---|---|
| Etapa atual | F validada no Windows — próxima G (frame pacing) |
| Última atualização | 2026-09-25 |
| Build do nativo | `zig build` via `mise` (só `zig 0.16.0`) |
| Alvo | `x86_64-windows-gnu`, saída em `native/zig-out/bin/` |

## Camadas (docs por camada)

| Camada | Doc | Estado |
|---|---|---|
| `native/` — hook DLL, pacer, injector (C++) | `docs/NATIVE.md` | PoC em andamento (Etapas A–E feitas) |
| `core/` — Limiter API, profiles, detecção (Rust) | `docs/CORE.md` | Planejado (Etapa I) |
| `app/` — overlay/UI (Tauri) | `docs/TAURI.md` | Planejado (Etapa J) |

## 1. Objetivo

Alternativa simples e leve ao RTSS, com UX de console: jogando no PC transmitido
para a TV (só controle + Steam Big Picture), abrir um pequeno overlay por hotkey
do controle, trocar o limite de FPS e ligar/desligar — sem mouse/teclado.

Prioridade: **não sacrificar frame pacing** (intervalos uniformes, não só média
correta). Steam é fonte de detecção, nunca dependência do limiter.

## 2. Roadmap

- [x] **A — dx11-test mínimo** (janela Win32 + D3D11, `Present(0,0)` sem VSYNC,
      FPS no título). Validado via cross-compile.
- [x] **B — DLL esqueleto** (ATTACH/DETACH validados no Windows).
- [x] **C — hook de `IDXGISwapChain::Present`** (hook + review fixes de
      race/ABI, commitado).
- [x] **D — contar/medir Presents** (ring QPC + `Limiter_GetStats`,
      validado no Windows).
- [x] **E — limiter extremamente simples** (cap ingênuo, validado no Windows).
- [x] **F — medir FPS e frametime** (DLL-vs-app < 1%; baseline sob cap 60:
      `avg≈16.65ms`, `max≈31.6ms` sistemático por quantum do timer).
- [ ] **G — melhorar frame pacing** (QPC, deadlines absolutos, drift, sleep +
      espera de alta precisão).
- [ ] **H — testar 30/40/45/60/72/90/120**.
- [ ] **I — núcleo em Rust** (`core/`).
- [ ] **J — UI Tauri** (overlay minimalista, sem roubar foco).
- [ ] **K — hotkey + gamepad**.
- [ ] **L — detecção Steam/processos**.
- [ ] **M — profiles** (memorizar último limite por jogo).
- [ ] **N — sugestões inteligentes** (pós-MVP).

MVP 1.0: toggle, slider, presets, hotkey, gamepad, detecção do processo,
iniciar com o Windows.

## 3. Decisões técnicas registradas

1. **Zig como compilador + build system; CMake removido.** `zig build`
   substitui `CMakeLists` + toolchains: 1 ferramenta em vez de 3, `build.zig`
   é código legível de cima a baixo. Histórico CMake preservado no git.
   System libs Win32 são linkadas manualmente (o CMake fazia implícito).
2. **Entry `WinMain` ANSI (não `wWinMain`)**: o CRT mingw resolve
   `WinMain` sem flags extras; linha de comando é ignorada.
3. **Sem link `d3dcompiler`**: o demo não compila shaders. Só `d3d11 + dxgi`.
4. **Subsystem GUI via `exe.subsystem = .Windows`** no `build.zig`.
5. **Zig linka libc++ (não libstdc++)**: ok para o PoC. Reavaliar se o hook
   futuro exigir ABI específica.
6. **Começar em C++, núcleo em Rust depois** (Etapa I): menos variáveis no PoC.
   O `core/` será Rust (vira crate usada direto pelo Tauri, zero FFI);
   o hook em `native/` fica C++. Zig é compilador, não linguagem do produto.
7. **Sem Steam/profiles/Tauri/gamepad/DX12/Vulkan até o pacing estar provado.**

## 4. Como trabalhar (protocolo do agente)

Por etapa: 1) objetivo, 2) arquivos criados/modificados, 3) código completo,
4) como compilar, 5) como executar, 6) o que observar p/ sucesso,
7) diagnosticar antes de avançar. Decisões arquiteturais: apresentar opções +
recomendação justificada. **Não antecipar funcionalidades futuras. Não pular
etapas.** Protótipo pequeno e mensurável primeiro:
*"Consigo injetar, interceptar Present e controlar o intervalo?"* →
*"O pacing é suficientemente bom?"* → só então o produto.

## 5. Segurança e estabilidade

DLL injection/hooking **somente no nosso `dx11-test`** durante o PoC. Sem
contornar anti-cheat, DRM ou proteções de terceiros. Uso em jogos reais só
depois, de forma responsável e compatível.
