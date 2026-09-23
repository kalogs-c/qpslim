// Present hook (Stage C): swaps the Present slot in the swapchain's
// vtable, forwards every call to the saved original. No limiting yet.
#pragma once

#include <dxgi.h>

// Patches vtable slot 8 of the given swapchain. Returns false if already
// hooked or the patch fails. Safe to call with nullptr (returns false).
bool HookSwapChain(IDXGISwapChain* swapchain);

// Restores the original slot. Safe to call when not hooked.
void UnhookSwapChain();
