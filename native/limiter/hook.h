#pragma once

#include <dxgi.h>

#include "../common/stats.h"

bool HookSwapChain(IDXGISwapChain* swapchain);

void UnhookSwapChain();

bool GetPresentStats(LimiterStats* out);

void SetTargetFps(int fps);
