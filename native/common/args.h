// CLI parsing for the test harness: no Win32, no allocation.
// Pure so the host unit tests cover it.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define FPS_DEFAULT 60
#define FPS_MAX 1000

// Parses a target FPS from a command-line string: leading integer in
// [1, FPS_MAX]. Anything else (empty, garbage, out of range) -> fallback.
int ParseTargetFps(const char* arg, int fallback);

#ifdef __cplusplus
}
#endif
