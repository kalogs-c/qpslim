// Protected pointer write for vtable slots and IAT cells.
// Single helper for hook.cpp and discover.cpp. Include after <windows.h>.
#pragma once

#include <windows.h>

inline bool WriteProtected(void** cell, void* value) {
  DWORD old_protect = 0;
  if (!VirtualProtect(cell, sizeof(void*), PAGE_READWRITE, &old_protect)) {
    return false;
  }
  *cell = value;
  if (!VirtualProtect(cell, sizeof(void*), old_protect, &old_protect)) {
    OutputDebugStringA("limiter: warning, page left writable");
  }
  FlushInstructionCache(GetCurrentProcess(), cell, sizeof(void*));
  return true;
}
