# Tauri — overlay/UI (planejado, Etapa J)

> Status: **não existe código ainda**.

## Papel

Interface propositalmente minimalista (conceito):

```text
┌───────────────────────┐
│ FPS LIMITER           │
│ ● ON                  │
│       60 FPS          │
│ ────────●────────     │
│ 30  40  45  60  90    │
└───────────────────────┘
```

## Requisitos

* Janela sem borda, pequena, overlay no canto; transparente quando apropriado.
* **Nunca roubar foco do jogo** desnecessariamente.
* Recursos do MVP: toggle ON/OFF, slider, presets, hotkey global, gamepad,
  detecção do jogo em execução, troca de limite sem mouse/teclado, iniciar
  com o Windows (opção).
* A UI só fala com o `core/` (ver `docs/CORE.md`) — nunca com o backend
  nativo diretamente.
