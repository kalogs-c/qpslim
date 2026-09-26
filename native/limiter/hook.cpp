// Present hook via vtable slot patch.
#include "hook.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

// Present is vtable slot 8: IUnknown (0-2), IDXGIObject (3-6),
// GetDevice (7), Present (8).
#define PRESENT_VTABLE_INDEX 8
#define LOG_EVERY_N_PRESENTS 300
#define STATS_WINDOW 240

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
// Naive cap (Stage E): frame interval in ticks, 0 = unlimited.
uint64_t g_interval_ticks = 0;
uint64_t g_next_deadline = 0;

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

// Deliberately naive: coarse Sleep, truncated sub-ms, no drift correction.
// Good pacing is Stage G's problem.
void WaitForDeadline(uint64_t now) {
  if (g_interval_ticks == 0) {
    return;
  }
  if (g_next_deadline == 0) {
    g_next_deadline = now + g_interval_ticks;
    return;
  }
  if (now < g_next_deadline) {
    uint64_t wait_ms =
        (g_next_deadline - now) * 1000 / g_qpc_freq;
    if (wait_ms > 0) {
      Sleep(static_cast<DWORD>(wait_ms));
    }
  }
  g_next_deadline += g_interval_ticks;
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
  if (fps <= 0 || g_qpc_freq == 0) {
    g_interval_ticks = 0;
  } else {
    g_interval_ticks = g_qpc_freq / static_cast<uint64_t>(fps);
  }
  g_next_deadline = 0;
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
