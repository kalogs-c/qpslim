// Swapchain discovery (Stage J): catch swapchains at creation so games
// need no explicit setter. Two independent nets:
//   - IDXGIFactory::CreateSwapChain vtable patch (same technique as Present);
//   - IAT patch of D3D11CreateDeviceAndSwapChain in every loaded module.
// Either path ends in HookSwapChain + the stored target. Reversal is
// explicit; never from DllMain.
//
// Limits: delay-loaded d3d11 is missed (IAT scanned once); swapchains that
// already exist at install time are never seen — inject early or recreate
// the device (resolution/fullscreen toggle).
#pragma once

// Hooks creation paths. False when already hooked or patching failed.
bool HookSwapChainCreation();

// Restores factory slot and IAT cells. Safe to call when not hooked.
void UnhookSwapChainCreation();
