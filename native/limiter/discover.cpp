// Swapchain discovery. See discover.h.
#include "discover.h"

#include "hook.h"
#include "mem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <tlhelp32.h>

// Verified against dxgi.h/dxgi1_2.h vtable order (0-based):
// IDXGIFactory::CreateSwapChain = 10, IDXGIFactory2::CreateSwapChainForHwnd = 15.
#define FACTORY_CREATE_SWAP_CHAIN_INDEX 10
#define FACTORY_CREATE_SWAP_CHAIN_FOR_HWND_INDEX 15
#define MAX_IAT_PATCHES 64

namespace {

typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainFn)(IDXGIFactory*,
                                                      IUnknown*,
                                                      DXGI_SWAP_CHAIN_DESC*,
                                                      IDXGISwapChain**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwndFn)(
    IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(WINAPI* D3D11CreateFn)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
    UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
typedef HRESULT(WINAPI* D3D11CreateDeviceFn)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
    UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

void** g_factory_vtable = nullptr;
void** g_factory2_vtable = nullptr;
CreateSwapChainFn g_original_create = nullptr;
CreateSwapChainForHwndFn g_original_create_hwnd = nullptr;
D3D11CreateFn g_original_create_device = nullptr;

struct IatPatch {
  void** cell;
  void* original;
};
IatPatch g_iat_patches[MAX_IAT_PATCHES] = {};
int g_iat_count = 0;

void DiscoverSwapChain(IDXGISwapChain* swapchain) {
  if (!swapchain) {
    return;
  }
  if (HookSwapChain(swapchain)) {
    OutputDebugStringA("limiter: discovered swapchain, present hooked");
  }
}

HRESULT STDMETHODCALLTYPE HookedCreateSwapChain(
    IDXGIFactory* self, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** out) {
  HRESULT hr = g_original_create(self, device, desc, out);
  if (SUCCEEDED(hr) && out) {
    DiscoverSwapChain(*out);
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE HookedCreateSwapChainForHwnd(
    IDXGIFactory2* self, IUnknown* device, HWND hwnd,
    const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
    IDXGIOutput* output, IDXGISwapChain1** out) {
  HRESULT hr = g_original_create_hwnd(self, device, hwnd, desc,
                                      fullscreen_desc, output, out);
  if (SUCCEEDED(hr) && out) {
    DiscoverSwapChain(*out);
  }
  return hr;
}

HRESULT WINAPI HookedCreateDeviceAndSwapChain(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver, HMODULE software,
    UINT flags, const D3D_FEATURE_LEVEL* levels, UINT level_count,
    UINT sdk, const DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swapchain,
    ID3D11Device** device, D3D_FEATURE_LEVEL* obtained, ID3D11DeviceContext** context) {
  if (!g_original_create_device) {
    return E_UNEXPECTED;
  }
  HRESULT hr = g_original_create_device(adapter, driver, software, flags,
                                        levels, level_count, sdk, desc,
                                        swapchain, device, obtained, context);
  if (SUCCEEDED(hr) && swapchain) {
    DiscoverSwapChain(*swapchain);
  }
  return hr;
}

bool SameText(const char* a, const char* b) {
  return _stricmp(a, b) == 0;
}

void PatchModuleImports(BYTE* base, FARPROC real_create) {
  IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return;
  }
  IMAGE_NT_HEADERS* nt =
      reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
    return;
  }
  DWORD dir_rva = nt->OptionalHeader
                      .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                      .VirtualAddress;
  if (dir_rva == 0) {
    return;
  }
  IMAGE_IMPORT_DESCRIPTOR* imp =
      reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir_rva);
  for (; imp->Name != 0; imp++) {
    const char* dll = reinterpret_cast<const char*>(base + imp->Name);
    if (!SameText(dll, "d3d11.dll")) {
      continue;
    }
    IMAGE_THUNK_DATA* name_thunk =
        imp->OriginalFirstThunk
            ? reinterpret_cast<IMAGE_THUNK_DATA*>(base +
                                                  imp->OriginalFirstThunk)
            : nullptr;
    IMAGE_THUNK_DATA* addr_thunk =
        reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
    for (int i = 0; addr_thunk[i].u1.Function != 0; i++) {
      bool match = false;
      if (name_thunk && !(name_thunk[i].u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
        const char* name = reinterpret_cast<const char*>(
            base + name_thunk[i].u1.AddressOfData + 2);
        match = SameText(name, "D3D11CreateDeviceAndSwapChain");
      } else {
        match = reinterpret_cast<uintptr_t>(
                    reinterpret_cast<void*>(addr_thunk[i].u1.Function)) ==
                reinterpret_cast<uintptr_t>(real_create);
      }
      if (!match || g_iat_count >= MAX_IAT_PATCHES) {
        continue;
      }
      void** cell = reinterpret_cast<void**>(&addr_thunk[i].u1.Function);
      void* original = reinterpret_cast<void*>(addr_thunk[i].u1.Function);
      if (WriteProtected(cell, reinterpret_cast<void*>(
                                  &HookedCreateDeviceAndSwapChain))) {
        g_iat_patches[g_iat_count++] = {cell, original};
      }
    }
  }
}

// Hidden window, just real enough for a throwaway swapchain.
const wchar_t* kBootstrapClass = L"QpslimBootstrap";

HWND CreateHiddenWindow(HINSTANCE inst) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = inst;
  wc.lpszClassName = kBootstrapClass;
  RegisterClassExW(&wc); // May already exist; CreateWindow decides.
  return CreateWindowExW(0, kBootstrapClass, L"", WS_POPUP, 0, 0, 16, 16,
                         nullptr, nullptr, inst, nullptr);
}

// Learns a swapchain class first-hand so late injection hooks instances
// created before install. Throwaway device + swapchain, released after.
void BootstrapLegacy(HMODULE d3d11, HWND hwnd) {
  auto create = reinterpret_cast<D3D11CreateFn>(
      GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain"));
  if (!create) {
    return;
  }
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  IDXGISwapChain* sc = nullptr;
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  if (SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                       0, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr,
                       &ctx))) {
    if (sc) {
      if (HookSwapChain(sc)) {
        OutputDebugStringA("limiter: bootstrapped legacy swapchain class");
      }
      sc->Release();
    }
    if (ctx) {
      ctx->Release();
    }
    if (dev) {
      dev->Release();
    }
  }
}

void BootstrapModern(HMODULE d3d11, IDXGIFactory* factory, HWND hwnd) {
  auto create_device = reinterpret_cast<D3D11CreateDeviceFn>(
      GetProcAddress(d3d11, "D3D11CreateDevice"));
  if (!create_device) {
    return;
  }
  IDXGIFactory2* factory2 = nullptr;
  ID3D11Device* dev = nullptr;
  if (FAILED(factory->QueryInterface(
          __uuidof(IDXGIFactory2),
          reinterpret_cast<void**>(&factory2))) ||
      !factory2 ||
      FAILED(create_device(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                           nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr,
                           nullptr)) ||
      !dev) {
    if (factory2) {
      factory2->Release();
    }
    return;
  }
  DXGI_SWAP_CHAIN_DESC1 sd1 = {};
  sd1.BufferCount = 2;
  sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  sd1.SampleDesc.Count = 1;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs = {};
  fs.Windowed = TRUE;
  IDXGISwapChain1* sc1 = nullptr;
  if (SUCCEEDED(factory2->CreateSwapChainForHwnd(dev, hwnd, &sd1, &fs, nullptr,
                                                 &sc1)) &&
      sc1) {
    if (HookSwapChain(sc1)) {
      OutputDebugStringA("limiter: bootstrapped ForHwnd swapchain class");
    }
    sc1->Release();
  }
  dev->Release();
  factory2->Release();
}

void PatchAllImports(FARPROC real_create) {
  HANDLE snap =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                               GetCurrentProcessId());
  if (snap != INVALID_HANDLE_VALUE) {
    MODULEENTRY32W me = {};
    me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
      do {
        PatchModuleImports(me.modBaseAddr, real_create);
      } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
  }
  char msg[64];
  snprintf(msg, sizeof(msg), "limiter: IAT patched in %d module(s)",
           g_iat_count);
  OutputDebugStringA(msg);
}

void BootstrapClasses(HMODULE d3d11, IDXGIFactory* factory) {
  // Before patching factory slots: throwaway swapchains teach us their
  // classes directly, so late injection hooks existing instances.
  // (Patch-first would route these creations through our own wrappers.)
  HINSTANCE inst = GetModuleHandleW(nullptr);
  HWND hwnd = CreateHiddenWindow(inst);
  if (hwnd) {
    BootstrapLegacy(d3d11, hwnd);
    BootstrapModern(d3d11, factory, hwnd);
    DestroyWindow(hwnd);
    UnregisterClassW(kBootstrapClass, inst);
  }
}

bool PatchFactorySlots(IDXGIFactory* factory) {
  void** vtable = *reinterpret_cast<void***>(factory);
  void* original = vtable[FACTORY_CREATE_SWAP_CHAIN_INDEX];
  if (!WriteProtected(&vtable[FACTORY_CREATE_SWAP_CHAIN_INDEX],
                      reinterpret_cast<void*>(&HookedCreateSwapChain))) {
    return false;
  }
  g_factory_vtable = vtable;
  g_original_create = reinterpret_cast<CreateSwapChainFn>(original);
  OutputDebugStringA("limiter: factory CreateSwapChain hooked");

  IDXGIFactory2* factory2 = nullptr;
  if (SUCCEEDED(factory->QueryInterface(
          __uuidof(IDXGIFactory2),
          reinterpret_cast<void**>(&factory2))) &&
      factory2) {
    void** vtable2 = *reinterpret_cast<void***>(factory2);
    void* original2 = vtable2[FACTORY_CREATE_SWAP_CHAIN_FOR_HWND_INDEX];
    if (WriteProtected(&vtable2[FACTORY_CREATE_SWAP_CHAIN_FOR_HWND_INDEX],
                       reinterpret_cast<void*>(
                           &HookedCreateSwapChainForHwnd))) {
      g_factory2_vtable = vtable2;
      g_original_create_hwnd =
          reinterpret_cast<CreateSwapChainForHwndFn>(original2);
      OutputDebugStringA("limiter: factory CreateSwapChainForHwnd hooked");
    }
    factory2->Release();
  }
  return true;
}

} // namespace

bool HookSwapChainCreation() {
  if (g_factory_vtable) {
    return false;
  }
  HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
  HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
  if (!dxgi || !d3d11) {
    return false;
  }
  auto create_factory = reinterpret_cast<decltype(&CreateDXGIFactory)>(
      GetProcAddress(dxgi, "CreateDXGIFactory"));
  FARPROC real_create =
      GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
  if (!create_factory || !real_create) {
    return false;
  }
  g_original_create_device = reinterpret_cast<D3D11CreateFn>(real_create);
  IDXGIFactory* factory = nullptr;
  if (FAILED(create_factory(__uuidof(IDXGIFactory),
                            reinterpret_cast<void**>(&factory))) ||
      !factory) {
    return false;
  }
  PatchAllImports(real_create);
  BootstrapClasses(d3d11, factory);

  if (!PatchFactorySlots(factory)) {
    factory->Release();
    return false;
  }
  factory->Release();
  return true;
}

void UnhookSwapChainCreation() {
  if (g_factory_vtable && g_original_create) {
    WriteProtected(&g_factory_vtable[FACTORY_CREATE_SWAP_CHAIN_INDEX],
                   reinterpret_cast<void*>(g_original_create));
    g_factory_vtable = nullptr;
    g_original_create = nullptr;
  }
  if (g_factory2_vtable && g_original_create_hwnd) {
    WriteProtected(&g_factory2_vtable[FACTORY_CREATE_SWAP_CHAIN_FOR_HWND_INDEX],
                   reinterpret_cast<void*>(g_original_create_hwnd));
    g_factory2_vtable = nullptr;
    g_original_create_hwnd = nullptr;
  }
  for (int i = 0; i < g_iat_count; i++) {
    WriteProtected(g_iat_patches[i].cell, g_iat_patches[i].original);
  }
  g_iat_count = 0;
  OutputDebugStringA("limiter: creation hooks removed");
}
