// limiter.dll — version + hook control + frame cap.
// The hook itself lives in hook.cpp. Win32 API only (no STL):
// keeps libc/libc++ out of the DLL.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hook.h"

#define LIMITER_VERSION 1

extern "C" __declspec(dllexport) int Limiter_GetVersion() {
  return LIMITER_VERSION;
}

extern "C" __declspec(dllexport) BOOL Limiter_HookSwapChain(
    IDXGISwapChain* swapchain) {
  return HookSwapChain(swapchain) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) void Limiter_UnhookSwapChain() {
  UnhookSwapChain();
}

extern "C" __declspec(dllexport) BOOL Limiter_GetStats(LimiterStats* out) {
  return GetPresentStats(out) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) void Limiter_SetTargetFps(int fps) {
  SetTargetFps(fps);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(inst);
    OutputDebugStringA("limiter: DLL_PROCESS_ATTACH");
  } else if (reason == DLL_PROCESS_DETACH) {
    // Log only: patching under the loader lock risks deadlock.
    // Reversal is the host's job, via Limiter_UnhookSwapChain.
    OutputDebugStringA("limiter: DLL_PROCESS_DETACH");
  }
  return TRUE;
}
