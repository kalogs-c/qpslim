// Pure stats math. See stats.h. No Win32 here on purpose: this file
// compiles for Windows (limiter.dll) and for the host (unit tests).
#include "stats.h"

bool WindowBounds(uint64_t count, uint64_t window_size, uint64_t* first,
                  uint64_t* n) {
  if (!first || !n) {
    return false;
  }
  // count-1 deltas stored (the first Present only arms the clock).
  if (count < 2) {
    return false;
  }
  uint64_t stored = count - 1;
  *n = stored < window_size ? stored : window_size;
  *first = count - *n + 1;
  return true;
}

bool StatsCompute(const uint64_t* deltas, uint64_t n, uint64_t freq,
                  uint64_t count, LimiterStats* out) {
  if (!deltas || !out || n == 0 || freq == 0) {
    return false;
  }
  uint64_t sum = 0;
  uint64_t max = 0;
  for (uint64_t i = 0; i < n; i++) {
    sum += deltas[i];
    if (deltas[i] > max) {
      max = deltas[i];
    }
  }
  double avg_ticks = static_cast<double>(sum) / static_cast<double>(n);
  double f = static_cast<double>(freq);
  out->present_count = count;
  out->fps_avg = f / avg_ticks;
  out->frametime_avg_ms = avg_ticks * 1000.0 / f;
  out->frametime_max_ms = static_cast<double>(max) * 1000.0 / f;
  return true;
}
