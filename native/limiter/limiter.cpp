// limiter.dll — Stage B skeleton. Loads into the target process,
// runs DllMain, exposes one export. No hook yet.
// Win32 API only (no STL): keeps libc/libc++ out of the DLL.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define LIMITER_VERSION 1

extern "C" __declspec(dllexport) int Limiter_GetVersion() {
  return LIMITER_VERSION;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(inst);
    OutputDebugStringA("limiter: DLL_PROCESS_ATTACH");
  } else if (reason == DLL_PROCESS_DETACH) {
    OutputDebugStringA("limiter: DLL_PROCESS_DETACH");
  }
  return TRUE;
}
