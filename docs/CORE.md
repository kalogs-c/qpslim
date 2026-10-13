# Core — núcleo do produto (Etapa K validada)

> Status: **núcleo com controle via pipe**. Crate `qpslim-core`, usada
> direto pelo Tauri, zero FFI. Profiles/detecção/settings seguem
> planejados (Etapas M–O).

## Papel

Lógica do produto entre a UI e o backend nativo. Não fala Win32/COM
diretamente — fala com a `limiter.dll` injetada pelo pipe
`\\.\pipe\qpslim-<pid>` (ver `docs/NATIVE.md`).

## Protocolo v1 (JSON-lines, `\n`, 4 KB por linha)

```text
→ {"v":1,"op":"set_fps","fps":60}    → {"v":1,"ok":true}
→ {"v":1,"op":"get_stats"}           → {"v":1,"ok":true,"present_count":N,
                                         "fps_avg":x,"frametime_avg_ms":y,
                                         "frametime_max_ms":z}
→ erro                               → {"v":1,"ok":false,"err":"bad_fps|bad_op|bad_version|no_stats"}
```

`fps: 0` = livre; `1..=1000`. Bytes exatos travados por testes dos dois
lados (`core/tests/proto_roundtrip.rs` + `common/pipemsg_test.zig`) —
o parser C não tem lib JSON e depende do formato.

## API Rust (`core/src/`)

* `proto`: `Request::{SetFps, GetStats}` (+ `encode` byte-exato),
  `Reply::{Ok, Stats(StatsReply)}`, `decode_reply`, `pipe_name(pid)`,
  consts `PROTOCOL_VERSION`/`FPS_MAX`/`DEFAULT_TARGET_FPS`/`MAX_LINE`.
* `client`: trait `Transport` (seam de teste), `PipeTransport`
  (Windows real; fora do Windows retorna `Unsupported`), `LimiterClient`
  com `set_fps` / `stats` / `enable(on)` — o toggle ON/OFF vive aqui
  (`off` envia `0` sem esquecer; `on` reenvia o último alvo).
* `ClientError::{Io, Proto}` — erro único para a futura camada Tauri.
* `examples/set_fps.rs`: `set_fps <pid> <fps|on|off|stats>`.

## Responsabilidades futuras

* **Profiles**: memorizar o último limite por jogo, sem configuração complexa.
* **Settings**: persistência de preferências (ex.: iniciar com o Windows).
* **Detecção**: processo/executável → PID; Steam AppID como fonte auxiliar
  (nunca dependência arquitetural).
* **Estatísticas**: percentis, 1% low, estabilidade de frametime —
  base das sugestões inteligentes (pós-MVP).

## Por que Rust (e não Zig/C++ aqui)

Mesma linguagem do app Tauri → o core é importado como crate, sem FFI.
Ecossistema (`serde` p/ config), testes unitários puros sem Windows/D3D, e
estabilidade 1.0 para código de longo prazo.
