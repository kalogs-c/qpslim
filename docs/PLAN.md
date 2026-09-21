# FPS Limiter para Windows — Plano do Projeto

> Documento vivo de contexto. **Atualizar `Status` e `Roadmap` ao concluir cada etapa.**
> Idioma do projeto: português. Alvo final: Windows 10/11.

## Status

| Campo | Valor |
|---|---|
| Etapa atual | A concluída — próximo: **B (DLL esqueleto carregável)** |
| Última atualização | 2026-09-21 |
| Toolchain padrão (Linux) | Zig via `mise` (`mise run build`) |
| Toolchain fallback (Linux) | MinGW-w64 via apt (`cmake/toolchain-mingw64.cmake`) |
| Toolchain nativo (Windows) | VS2022 + MSVC (sem toolchain file) |

## 1. Objetivo

Alternativa simples e leve ao RTSS, com UX de console: jogando no PC transmitido
para a TV (só controle + Steam Big Picture), abrir um pequeno overlay por hotkey
do controle, trocar o limite de FPS e ligar/desligar — sem mouse/teclado.

Prioridade: **não sacrificar frame pacing**. Meta técnica = comportamento de
limiter próximo a RTSS/SteamOS (intervalos uniformes, não só média correta).

Ex.: 60 FPS ideal = `16.66 16.67 16.66 …`, nunca `16.1 18.3 15.7 …`.

## 2. Visão de longo prazo

```text
Tauri UI (overlay / slider / presets / hotkeys / gamepad)
  → Rust core (detecção, profiles, settings, Limiter API)
    → Platform backend (DX11 / DX12 / Vulkan, frame pacing)
```

* Steam é **fonte de detecção**, não dependência arquitetural do limiter.
* Cobertura alvo: jogos Steam, Non-Steam Games, outros launchers, qualquer
  processo identificável — fallback sempre por processo/executável.

## 3. Roadmap

- [x] **A — dx11-test mínimo** (janela Win32 + D3D11, `Present(0,0)` sem VSYNC,
      FPS no título). Validado via cross-compile.
- [ ] **B — DLL esqueleto** (carrega no processo, `OutputDebugString`, sem hook).
- [ ] **C — hook de `IDXGISwapChain::Present`**.
- [ ] **D — contar/medir Presents**.
- [ ] **E — limiter extremamente simples** (só provar o conceito).
- [ ] **F — medir FPS e frametime**.
- [ ] **G — melhorar frame pacing** (QPC, deadlines absolutos, drift, sleep +
      espera de alta precisão).
- [ ] **H — testar 30/40/45/60/72/90/120**.
- [ ] **I — portar núcleo para Rust**.
- [ ] **J — UI Tauri** (overlay minimalista, sem borda, sem roubar foco).
- [ ] **K — hotkey + gamepad**.
- [ ] **L — detecção Steam/processos**.
- [ ] **M — profiles** (memorizar último limite por jogo, sem config complexa).
- [ ] **N — sugestões inteligentes** (média, 1% low, percentis, VRR — pós-MVP).

Critério da Etapa A (atingido): app DX11 própria, `Present` sem VSYNC com FPS
alto observável, sem crash, resize e ESC funcionais.

## 4. MVP 1.0 / Pós-MVP (resumo)

MVP: toggle ON/OFF, slider, presets (30/40/45/60/90…), hotkey global, gamepad,
detecção do processo, troca de limite sem mouse/teclado, iniciar com Windows.
UI conceitual: overlay pequeno com status + slider + presets.

Pós-MVP: profiles automáticos por jogo; detecção via Steam AppID→processo→PID;
sugestões de limite baseadas em média/1% low/estabilidade (nunca só média).

## 5. Arquitetura desejada

```text
Tauri → Limiter API (enable/disable/set_target_fps) → Frame Pacer → Platform backend
```

A UI não conhece o backend. Troca de implementação interna sem reescrever a UI.

## 6. Toolchain e workflow

Dependências gerenciadas por **`mise`** (`mise.toml`): `cmake 4.4.3`,
`ninja 1.13.2`, `zig 0.16.0`.

```bash
mise install          # uma vez (requer `mise trust` no primeiro uso)
mise run build        # configura com toolchain Zig + compila
mise run clean        # remove build/
```

| Ambiente | Comando |
|---|---|
| Linux (padrão, sem sudo) | `mise run build` → `build/dx11-test/dx11-test.exe` |
| Linux fallback (apt, com sudo) | `cmake -B build --toolchain cmake/toolchain-mingw64.cmake -G Ninja && cmake --build build` |
| Windows VS2022 | `cmake -B build && cmake --build build --config Release` |

Validação do `.exe` no Linux (sem `file`/binutils): script Python checa
`MZ` + assinatura `PE` + machine `0x8664` + subsystem GUI(2) + símbolo
`D3D11CreateDeviceAndSwapChain`. Execução real só no Windows 10/11:
janela 1280x720 pulsante + `FPS: NNN` no título, resize ok, ESC fecha.

Estrutura atual:

```text
.
├── mise.toml
├── CMakeLists.txt
├── cmake/toolchain-zig.cmake        # padrão
├── cmake/toolchain-mingw64.cmake    # fallback (suporta $MINGW_ROOT)
├── dx11-test/main.cpp
└── docs/PLAN.md                     # este arquivo
```

## 7. Decisões técnicas registradas

1. **Zig como cross-compiler padrão** (não MinGW via apt): sem sudo, headers e
   libs `windows-gnu` embutidos, reprodutível via `mise`. MinGW-GCC mantido
   como fallback — ambos compilam hoje.
2. **Entry `WinMain` ANSI (não `wWinMain`)**: o CRT do Zig só resolve
   `WinMain`; linha de comando é ignorada. Remove a necessidade do `-municode`
   do GCC. Vale nos 3 toolchains.
3. **Sem link `d3dcompiler`**: o demo não compila shaders; o Zig não empacota
   esse import lib. Só `d3d11 + dxgi`.
4. **Subsystem GUI forçado no Zig** (`LINKER:--subsystem,windows`): o driver do
   Zig ignora o `-mwindows` que o CMake emite para `WIN32`.
5. **`zig c++` = libc++, não libstdc++**: ok para o PoC (só `cmath/cstdio/string`
   + API C do D3D). Reavaliar se o hook futuro exigir ABI específica.
6. **Começar em C++, portar para Rust depois** (Etapa I): menos variáveis no PoC.
7. **Sem Steam/profiles/Tauri/gamepad/DX12/Vulkan até o pacing estar provado.**

## 8. Como trabalhar (protocolo do agente)

Por etapa: 1) objetivo, 2) arquivos criados/modificados, 3) código completo,
4) como compilar, 5) como executar, 6) o que observar p/ sucesso,
7) diagnosticar antes de avançar. Decisões arquiteturais: apresentar opções +
recomendação justificada. **Não antecipar funcionalidades futuras. Não pular
etapas.** Protótipo pequeno e mensurável primeiro:
*"Consigo injetar, interceptar Present e controlar o intervalo?"* →
*"O pacing é suficientemente bom?"* → só então o produto.

## 9. Segurança e estabilidade

DLL injection/hooking **somente no nosso `dx11-test`** durante o PoC. Sem
contornar anti-cheat, DRM ou proteções de terceiros. Uso em jogos reais só
depois, de forma responsável e compatível.
