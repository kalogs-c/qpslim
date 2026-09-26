// Pure stats math: no Win32, no allocation. Shared by limiter.dll and
// the host unit tests. C ABI so Zig tests (and a future Rust core)
// can call it directly.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Writable by C callers: plain C layout, fixed-size types only.
struct LimiterStats {
  uint64_t present_count;
  double fps_avg;
  double frametime_avg_ms;
  double frametime_max_ms;
};

// Recent window over `count` total samples: `first` is the 1-based index of
// the oldest sample in the window, `n` its size. False when empty.
bool WindowBounds(uint64_t count, uint64_t window_size, uint64_t* first,
                  uint64_t* n);

// Stats over `n` deltas (ticks) with QPC `freq`. False when empty.
bool StatsCompute(const uint64_t* deltas, uint64_t n, uint64_t freq,
                  uint64_t count, LimiterStats* out);

#ifdef __cplusplus
}
#endif
