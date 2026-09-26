// Pure pacer math: no Win32, no allocation. Shared by limiter.dll and
// the host unit tests. C ABI so Zig tests can call it directly.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Whole ms to Sleep before spinning: the bulk of (deadline - now) minus
// the spin margin. 0 when late or already inside the spin zone.
uint64_t SleepMsFor(uint64_t now, uint64_t deadline, uint64_t freq,
                    uint64_t spin_margin_ticks);

// Next deadline after presenting: advances by interval until past now,
// skipping missed beats without catch-up bursts or debt.
uint64_t AdvanceDeadline(uint64_t deadline, uint64_t interval, uint64_t now);

#ifdef __cplusplus
}
#endif
