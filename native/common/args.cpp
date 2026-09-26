// See args.h. strtol-based: no exceptions, no allocation.
#include "args.h"

#include <stdlib.h>

int ParseTargetFps(const char* arg, int fallback) {
  if (!arg || !*arg) {
    return fallback;
  }
  char* end = nullptr;
  long value = strtol(arg, &end, 10);
  if (end == arg || *end != '\0' || value < 1 || value > FPS_MAX) {
    return fallback;
  }
  return static_cast<int>(value);
}
