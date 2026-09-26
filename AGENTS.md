# AGENTS.md — convenções do projeto

> Instruções para agentes (e humanos) trabalhando neste repo.
> Detalhes de arquitetura e roadmap: `docs/PLAN.md`, `docs/NATIVE.md`,
> `docs/CORE.md`, `docs/TAURI.md`.

## Idioma

* **Código 100% em inglês**: comentários, identificadores, strings de log,
  mensagens de erro, descriptions de tasks.
* **Docs em pt-BR**: `docs/`, `AGENTS.md`.

## Estilo de código (C++)

* **Chaves sempre** em `if`/`else`/`for`/`while`, mesmo com corpo de uma linha
  (inclusive guardas de retorno antecipado).
* **Comentários mínimos**: só o que tem valor real ("porquê", nunca "o quê").
  Sem comentário óbvio, sem código morto, sem variáveis não usadas.
* Preferir retornos antecipados e helpers pequenos (`TryLoadLimiter`,
  `TryHookPresent`) a aninhamento profundo.
* **Privado por padrão**: tudo dentro de `namespace {}`; só a API pública
  fica fora (além de `WinMain`/`DllMain`, exigidos pelo linker/loader).
* Legibilidade humana acima de tudo; manter simples e funcional.

## Restrições do `limiter/` (DLL injetada)

* Só Win32 API — **sem STL, sem `libc++`** (usa `<stdio.h>`, nunca `<cstdio>`).
* `DllMain` mínimo: só `DisableThreadLibraryCalls` + log. Nunca
  `VirtualProtect`/patch/unpatch sob loader lock (risco de deadlock) —
  a reversão é via chamada explícita do host antes de `FreeLibrary`.
* Todo patch precisa de reversão (ex.: `UnhookSwapChain` via chamada
  explícita do host antes de `FreeLibrary`).

## Build e validação

* Build do nativo: `mise run build` (da raiz). Nunca commitar sem rodar.
* Testes host: `mise run test` (matemática pura em Zig; hook/DLL seguem
  manuais no Windows).
* Após qualquer mudança no nativo: checar PE (`MZ`, x64, subsystem GUI,
  exports) antes de pedir validação no Windows.
* **Commits**: nunca commitar sem pedido explícito — o usuário valida os
  binários no Windows primeiro.
