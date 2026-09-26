# Tauri — overlay/UI (planejado, Etapa L)

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
* **Aviso anti-cheat obrigatório**: banner claro de risco de ban + recomendação
  de nunca usar em jogos com anti-cheat (EAC/BE/Vanguard). Injeção só sob ação
  do usuário ou em jogos permitidos; nunca tocar processo protegido.
