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
