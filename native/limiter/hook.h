// Present hook: swaps the Present slot in each swapchain class vtable,
// forwards every call to the saved original, paces frames to the target.
#pragma once

#include <dxgi.h>

#include "../common/stats.h"

// Patches vtable slot 8 of the given swapchain. Returns false if already
// hooked or the patch fails. Safe to call with nullptr (returns false).
bool HookSwapChain(IDXGISwapChain* swapchain);

// Restores the original slot. Safe to call when not hooked.
void UnhookSwapChain();

// Fills out with stats over the recent window. False when empty.
bool GetPresentStats(LimiterStats* out);

// Sets the frame rate cap in FPS. 0 (or negative) means unlimited.
// Takes effect on subsequent presents; re-arms the deadline.
void SetTargetFps(int fps);
