// Present hook via vtable slot patch.
#include "hook.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

// Present is vtable slot 8: IUnknown (0-2), IDXGIObject (3-6),
// GetDevice (7), Present (8).
#define PRESENT_VTABLE_INDEX 8
#define LOG_EVERY_N_PRESENTS 300

namespace {

typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(IDXGISwapChain*, UINT, UINT);

void** g_vtable = nullptr;
PresentFn g_original_present = nullptr;
LONG64 g_present_count = 0;

void LogCount(LONG64 count) {
  char msg[64];
  snprintf(msg, sizeof(msg), "limiter: present #%lld", count);
  OutputDebugStringA(msg);
}

HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* self, UINT sync,
                                        UINT flags) {
  // Snapshot: install may run concurrently on another thread.
  PresentFn original = g_original_present;
  LONG64 count = InterlockedIncrement64(&g_present_count);
  if (count % LOG_EVERY_N_PRESENTS == 0) {
    LogCount(count);
  }
  if (!original) {
    return E_UNEXPECTED;
  }
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
