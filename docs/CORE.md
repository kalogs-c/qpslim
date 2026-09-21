# Core — núcleo do produto (planejado, Etapa I)

> Status: **não existe código ainda**. Idioma planejado: Rust (crate usada
> direto pelo Tauri, zero FFI).

## Papel

Lógica do produto entre a UI e o backend nativo. Não fala Win32/COM
diretamente — consome a Limiter API do `native/` (ver `docs/NATIVE.md`).

## Responsabilidades

* **Limiter API**: `enable()` / `disable()` / `set_target_fps(n)`.
* **Profiles**: memorizar o último limite por jogo, sem configuração complexa.
* **Settings**: persistência de preferências (ex.: iniciar com o Windows).
* **Detecção**: processo/executável → PID; Steam AppID como fonte auxiliar
  (nunca dependência arquitetural).
* **Estatísticas**: FPS médio, percentis, 1% low, estabilidade de frametime —
  base das sugestões inteligentes (pós-MVP).

## Por que Rust (e não Zig/C++ aqui)

Mesma linguagem do app Tauri → o core é importado como crate, sem FFI.
Ecossistema (`serde` p/ config), testes unitários puros sem Windows/D3D, e
estabilidade 1.0 para código de longo prazo.
