// Either path ends in HookSwapChain + the stored target. Reversal is
// explicit; never from DllMain.
//
// Limits: delay-loaded d3d11 is missed (IAT scanned once). Swapchains
// that already exist at install time are covered via bootstrap
// throwaways (up to MAX_HOOKED_CLASSES); inject early or recreate
// the device only for delay-loaded modules.
#pragma once

bool HookSwapChainCreation();

void UnhookSwapChainCreation();
