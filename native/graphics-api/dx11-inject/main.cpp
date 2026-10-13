// inject-test: game-like harness for injector + discovery validation.
// Unlike dx11-test, it never loads limiter.dll and never calls the setter:
// the only way the hook engages is via injector + auto-discovery.
// Keys: C = create device (IAT path), V = create via factory (ForHwnd path),
// R = release + recreate, ESC = quit. Starts with no device.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <cmath>
#include <cstdio>

#if defined(_MSC_VER)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace {

HWND g_hwnd = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapchain = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
bool g_running = true;
bool g_create_requested = false;
bool g_factory_path = false;
bool g_recreate_requested = false;

void Log(const char* msg) {
  OutputDebugStringA(msg);
  OutputDebugStringA("\n");
}

void Quit() {
  g_running = false;
  PostQuitMessage(0);
}

bool CreateRenderTarget() {
  ID3D11Texture2D* backbuffer = nullptr;
  HRESULT hr = g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(&backbuffer));
  if (FAILED(hr)) {
    return false;
  }
  hr = g_device->CreateRenderTargetView(backbuffer, nullptr, &g_rtv);
  backbuffer->Release();
  if (FAILED(hr)) {
    return false;
  }
  g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
  return true;
}

void DestroyDevice() {
  if (g_context) {
    g_context->OMSetRenderTargets(0, nullptr, nullptr);
  }
  if (g_rtv) {
    g_rtv->Release();
    g_rtv = nullptr;
  }
  if (g_swapchain) {
    g_swapchain->Release();
    g_swapchain = nullptr;
  }
  if (g_context) {
    g_context->ClearState();
    g_context->Release();
    g_context = nullptr;
  }
  if (g_device) {
    g_device->Release();
    g_device = nullptr;
  }
}

bool CreateDeviceIAT(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
      D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, nullptr, &g_context);
  if (FAILED(hr) || !CreateRenderTarget()) {
    Log("inject-test: device creation (IAT path) failed");
    DestroyDevice();
    return false;
  }
  Log("inject-test: device created (IAT path)");
  return true;
}

bool CreateDeviceFactory(HWND hwnd) {
  IDXGIFactory2* factory2 = nullptr;
  if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory2),
                               reinterpret_cast<void**>(&factory2))) ||
      !factory2) {
    Log("inject-test: factory creation failed");
    return false;
  }
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                 nullptr, 0, D3D11_SDK_VERSION, &g_device,
                                 nullptr, &g_context);
  if (FAILED(hr) || !g_device) {
    Log("inject-test: device creation (factory path) failed");
    factory2->Release();
    return false;
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
  hr = factory2->CreateSwapChainForHwnd(g_device, hwnd, &sd1, &fs, nullptr,
                                        &sc1);
  factory2->Release();
  if (FAILED(hr) || !sc1) {
    Log("inject-test: swapchain creation (factory path) failed");
    DestroyDevice();
    return false;
  }
  g_swapchain = sc1;
  if (!CreateRenderTarget()) {
    DestroyDevice();
    return false;
  }
  Log("inject-test: device created (factory path)");
  return true;
}

bool HookState() {
  return GetModuleHandleW(L"limiter.dll") != nullptr;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY:
    Quit();
    return 0;
  case WM_KEYDOWN:
    if (wparam == VK_ESCAPE) {
      Quit();
      return 0;
    }
    if (wparam == 'C' || wparam == 'V') {
      g_factory_path = (wparam == 'V');
      g_create_requested = true;
      return 0;
    }
    if (wparam == 'R') {
      g_recreate_requested = true;
      return 0;
    }
    break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
  const wchar_t* kClass = L"QpslimInjectTest";

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
  wc.lpszClassName = kClass;
  if (!RegisterClassExW(&wc)) {
    return 1;
  }

  RECT rc{0, 0, 1280, 720};
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
  g_hwnd =
      CreateWindowExW(0, kClass, L"inject-test — C/V create, R recreate, ESC quit",
                      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                      rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr,
                      inst, nullptr);
  if (!g_hwnd) {
    return 1;
  }
  ShowWindow(g_hwnd, show);
  UpdateWindow(g_hwnd);
  Log("inject-test: waiting (C = IAT path, V = factory path)");

  LARGE_INTEGER freq{}, t0{}, t_title{};
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&t0);
  t_title = t0;
  UINT frames = 0;
  double elapsed = 0.0;

  MSG msg{};
  while (g_running) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        g_running = false;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (!g_running) {
      break;
    }
    if (g_recreate_requested) {
      g_recreate_requested = false;
      DestroyDevice();
      g_create_requested = true;
      Log("inject-test: device released, recreating");
    }
    if (g_create_requested) {
      g_create_requested = false;
      if (g_factory_path) {
        CreateDeviceFactory(g_hwnd);
      } else {
        CreateDeviceIAT(g_hwnd);
      }
    }
    if (!g_device) {
      Sleep(16);
      continue;
    }
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    elapsed = static_cast<double>(now.QuadPart - t0.QuadPart) /
              static_cast<double>(freq.QuadPart);

    float r = 0.5f + 0.5f * sinf(static_cast<float>(elapsed * 1.7));
    float g = 0.5f + 0.5f * sinf(static_cast<float>(elapsed * 2.3 + 2.1));
    float b = 0.5f + 0.5f * sinf(static_cast<float>(elapsed * 3.1 + 4.2));
    const float clear[4] = {r, g, b, 1.0f};
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_context->ClearRenderTargetView(g_rtv, clear);
    if (FAILED(g_swapchain->Present(0, 0))) {
      break;
    }
    frames++;

    double since_title =
        static_cast<double>(now.QuadPart - t_title.QuadPart) /
        static_cast<double>(freq.QuadPart);
    if (since_title >= 2.0) {
      double fps = frames / since_title;
      wchar_t title[160];
      _snwprintf_s(title, _countof(title), _TRUNCATE,
                   L"inject-test — %s FPS: %.0f (C/V/R/ESC)",
                   HookState() ? L"HOOKED" : L"CLEAN", fps);
      SetWindowTextW(g_hwnd, title);
      frames = 0;
      t_title = now;
    }
  }

  DestroyDevice();
  DestroyWindow(g_hwnd);
  return 0;
}
