// Pure pacer math. See pacer.h. No Win32 here on purpose: this file
// compiles for Windows (limiter.dll) and for the host (unit tests).
#include "pacer.h"

uint64_t SleepMsFor(uint64_t now, uint64_t deadline, uint64_t freq,
                    uint64_t spin_margin_ticks) {
  if (freq == 0 || now + spin_margin_ticks >= deadline) {
    return 0;
  }
  return (deadline - spin_margin_ticks - now) * 1000 / freq;
}

uint64_t AdvanceDeadline(uint64_t deadline, uint64_t interval, uint64_t now) {
  if (interval == 0) {
    return deadline;
  }
  uint64_t next = deadline + interval;
  while (next <= now) {
    next += interval;
  }
  return next;
}
