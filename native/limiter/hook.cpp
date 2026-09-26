// Present hook via vtable slot patch.
#include "hook.h"

#include "../common/pacer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

// Present is vtable slot 8: IUnknown (0-2), IDXGIObject (3-6),
// GetDevice (7), Present (8).
#define PRESENT_VTABLE_INDEX 8
#define LOG_EVERY_N_PRESENTS 300
#define STATS_WINDOW 240
#define SPIN_MARGIN_MS 2

namespace {

typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(IDXGISwapChain*, UINT, UINT);

void** g_vtable = nullptr;
PresentFn g_original_present = nullptr;
LONG64 g_present_count = 0;
// Ring buffer of QPC deltas (ticks, integer-only on the hot path).
// g_present_count doubles as total and as ring index (count % N).
uint64_t g_frametimes[STATS_WINDOW] = {};
uint64_t g_prev_ticks = 0;
uint64_t g_qpc_freq = 0;
// Frame cap interval in ticks, 0 = unlimited.
uint64_t g_interval_ticks = 0;
uint64_t g_next_deadline = 0;
// Last stretch covered by QPC spin, not Sleep.
uint64_t g_spin_margin_ticks = 0;
bool g_timer_high_res = false;

void LogCount(LONG64 count) {
  char msg[64];
  snprintf(msg, sizeof(msg), "limiter: present #%lld", count);
  OutputDebugStringA(msg);
}

void RecordPresentTick(uint64_t ticks, LONG64 count) {
  if (g_prev_ticks != 0) {
    g_frametimes[(count - 1) % STATS_WINDOW] = ticks - g_prev_ticks;
  }
  g_prev_ticks = ticks;
}

// Hybrid wait (Stage G): Sleep the bulk, QPC-spin the last stretch.
// Absolute deadlines with fast-forward: missed beats are skipped,
// never bursted, never owed.
void RequestTimerRes() {
  if (!g_timer_high_res && timeBeginPeriod(1) == TIMERR_NOERROR) {
    g_timer_high_res = true;
  }
}

void ReleaseTimerRes() {
  if (g_timer_high_res) {
    timeEndPeriod(1);
    g_timer_high_res = false;
  }
}

uint64_t SpinUntilDeadline(uint64_t deadline) {
  LARGE_INTEGER t = {};
  do {
    YieldProcessor();
    QueryPerformanceCounter(&t);
  } while (static_cast<uint64_t>(t.QuadPart) < deadline);
  return static_cast<uint64_t>(t.QuadPart);
}

void WaitForDeadline(uint64_t now) {
  if (g_interval_ticks == 0) {
    return;
  }
  if (g_next_deadline == 0) {
    g_next_deadline = now + g_interval_ticks;
    return;
  }
  if (now < g_next_deadline) {
    RequestTimerRes();
    uint64_t sleep_ms = SleepMsFor(now, g_next_deadline, g_qpc_freq,
                                   g_spin_margin_ticks);
    if (sleep_ms > 0) {
      Sleep(static_cast<DWORD>(sleep_ms));
    }
    now = SpinUntilDeadline(g_next_deadline);
  }
  g_next_deadline = AdvanceDeadline(g_next_deadline, g_interval_ticks, now);
}

HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* self, UINT sync,
                                        UINT flags) {
  // Snapshot: install may run concurrently on another thread.
  PresentFn original = g_original_present;
  LARGE_INTEGER now = {};
  QueryPerformanceCounter(&now);
  LONG64 count = InterlockedIncrement64(&g_present_count);
  RecordPresentTick(static_cast<uint64_t>(now.QuadPart), count);
  if (count % LOG_EVERY_N_PRESENTS == 0) {
    LogCount(count);
  }
  if (!original) {
    return E_UNEXPECTED;
  }
  WaitForDeadline(static_cast<uint64_t>(now.QuadPart));
  return original(self, sync, flags);
}

bool WriteVtableSlot(void** vtable, int index, void* value) {
  DWORD old_protect = 0;
  if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE,
                      &old_protect)) {
    return false;
  }
  vtable[index] = value;
  if (!VirtualProtect(&vtable[index], sizeof(void*), old_protect,
                      &old_protect)) {
    OutputDebugStringA("limiter: warning, vtable slot left writable");
  }
  FlushInstructionCache(GetCurrentProcess(), &vtable[index], sizeof(void*));
  return true;
}

} // namespace

void SetTargetFps(int fps) {
  g_next_deadline = 0;
  if (fps <= 0 || g_qpc_freq == 0) {
    g_interval_ticks = 0;
    ReleaseTimerRes();
    return;
  }
  g_interval_ticks = g_qpc_freq / static_cast<uint64_t>(fps);
  g_spin_margin_ticks = g_qpc_freq * SPIN_MARGIN_MS / 1000;
}

bool HookSwapChain(IDXGISwapChain* swapchain) {
  // Install happens once at startup; concurrent installs are the caller's job.
  // DXGI vtables are static per COM class, so one patch misses other classes
  // (e.g. IDXGISwapChain1 has its own vtable).
  if (!swapchain || g_vtable) {
    return false;
  }
  void** vtable = *reinterpret_cast<void***>(swapchain);
  // Store before patching: any thread reaching HookedPresent from here on
  // must see a valid original.
  g_vtable = vtable;
  g_original_present =
      reinterpret_cast<PresentFn>(vtable[PRESENT_VTABLE_INDEX]);
  LARGE_INTEGER freq = {};
  if (QueryPerformanceFrequency(&freq)) {
    g_qpc_freq = static_cast<uint64_t>(freq.QuadPart);
  }
  MemoryBarrier();
  if (!WriteVtableSlot(vtable, PRESENT_VTABLE_INDEX,
                       reinterpret_cast<void*>(&HookedPresent))) {
    g_vtable = nullptr;
    g_original_present = nullptr;
    return false;
  }
  OutputDebugStringA("limiter: present hooked, slot patched");
  return true;
}

void UnhookSwapChain() {
  if (!g_vtable) {
    return;
  }
  WriteVtableSlot(g_vtable, PRESENT_VTABLE_INDEX,
                  reinterpret_cast<void*>(g_original_present));
  g_vtable = nullptr;
  ReleaseTimerRes();
  // g_original_present intentionally kept: in-flight HookedPresent calls
  // still route through it, and DXGI code is always valid.
  OutputDebugStringA("limiter: present unhooked, slot restored");
}

bool GetPresentStats(LimiterStats* out) {
  if (!out || g_qpc_freq == 0) {
    return false;
  }
  // Single writer (hook), occasional reader: plain reads, diagnostics-only.
  uint64_t count = static_cast<uint64_t>(g_present_count);
  uint64_t first = 0;
  uint64_t n = 0;
  if (!WindowBounds(count, STATS_WINDOW, &first, &n)) {
    return false;
  }
  uint64_t window[STATS_WINDOW] = {};
  for (uint64_t p = first; p < first + n; p++) {
    window[p - first] = g_frametimes[(p - 1) % STATS_WINDOW];
  }
  return StatsCompute(window, n, g_qpc_freq, count, out);
}
