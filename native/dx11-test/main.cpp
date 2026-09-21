// dx11-test — Etapa A
// Alvo minimo DirectX 11 para o futuro hook de IDXGISwapChain::Present.
// Propositalmente simples: janela Win32 + clear color animado + FPS no titulo.
// VSYNC desligado (Present(0,0)) para gerar o maximo de Presents possivel.
//
// Build (via mise, da raiz):  mise run build   (saida: native/zig-out/bin/)
//   direto (da pasta native/): zig build
//   otimizado p/ medicao:     zig build -Doptimize=ReleaseFast

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cmath>
#include <cstdio>
#include <string>

// Link automático no MSVC. No MinGW/Zig o link vem do CMake (d3d11 dxgi).
#if defined(_MSC_VER)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace {

// Estado global mínimo (PoC, sem abstrações ainda).
HWND g_hwnd = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapchain = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
bool g_running = true;
bool g_device_ok = false;

void Log(const char* msg) {
  OutputDebugStringA(msg);
  OutputDebugStringA("\n");
}

void ShowHResultError(HRESULT hr, const wchar_t* what) {
  wchar_t buf[512];
  _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%ls falhou (HRESULT=0x%08lX)", what,
               static_cast<unsigned long>(hr));
  MessageBoxW(g_hwnd, buf, L"dx11-test", MB_OK | MB_ICONERROR);
}

bool CreateRenderTarget() {
  ID3D11Texture2D* backbuffer = nullptr;
  HRESULT hr = g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(&backbuffer));
  if (FAILED(hr)) {
    ShowHResultError(hr, L"GetBuffer");
    return false;
  }
  hr = g_device->CreateRenderTargetView(backbuffer, nullptr, &g_rtv);
  backbuffer->Release();
  if (FAILED(hr)) {
    ShowHResultError(hr, L"CreateRenderTargetView");
    return false;
  }
  g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
  return true;
}

void DestroyRenderTarget() {
  if (g_context) g_context->OMSetRenderTargets(0, nullptr, nullptr);
  if (g_rtv) {
    g_rtv->Release();
    g_rtv = nullptr;
  }
}

bool InitD3D(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;   // usa largura da janela
  sd.BufferDesc.Height = 0;  // usa altura da janela
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 0;  // irrelevante com Present(0,0)
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  sd.Flags = 0;

  UINT create_flags = 0;
#if defined(_DEBUG)
  create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                      D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
  D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_flags, levels, _countof(levels),
      D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, &obtained, &g_context);
  if (FAILED(hr)) {
    // Fallback WARP: permite rodar em VM sem GPU (lento, mas nao trava a Etapa A).
    Log("Hardware falhou, tentando WARP...");
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, create_flags, levels, _countof(levels),
        D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, &obtained, &g_context);
  }
  if (FAILED(hr)) {
    ShowHResultError(hr, L"D3D11CreateDeviceAndSwapChain");
    return false;
  }
  if (!CreateRenderTarget()) return false;

  g_device_ok = true;
  return true;
}

void ShutdownD3D() {
  DestroyRenderTarget();
  if (g_context) {
    g_context->ClearState();
    g_context->Release();
    g_context = nullptr;
  }
  if (g_swapchain) {
    g_swapchain->Release();
    g_swapchain = nullptr;
  }
  if (g_device) {
    g_device->Release();
    g_device = nullptr;
  }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_DESTROY:
      g_running = false;
      PostQuitMessage(0);
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) {
        g_running = false;
        PostQuitMessage(0);
        return 0;
      }
      break;
    case WM_SIZE:
      if (g_swapchain && wparam != SIZE_MINIMIZED) {
        DestroyRenderTarget();
        UINT w = LOWORD(lparam);
        UINT h = HIWORD(lparam);
        if (w == 0) w = 1;
        if (h == 0) h = 1;
        HRESULT hr = g_swapchain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        if (SUCCEEDED(hr)) {
          CreateRenderTarget();
        }
      }
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

// Entry ANSI de proposito (nao wWinMain): funciona identico nos tres
// toolchains (MSVC, GCC-MinGW e Zig) sem precisar da flag -municode
// do GCC. A linha de comando e ignorada; todo texto usa APIs W.
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
  const wchar_t* kClass = L"QpslimDx11Test";

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
  wc.lpszClassName = kClass;
  if (!RegisterClassExW(&wc)) {
    MessageBoxW(nullptr, L"RegisterClassEx falhou", L"dx11-test", MB_OK | MB_ICONERROR);
    return 1;
  }

  RECT rc{0, 0, 1280, 720};
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
  g_hwnd = CreateWindowExW(0, kClass, L"dx11-test — FPS: ...", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                           rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
  if (!g_hwnd) {
    MessageBoxW(nullptr, L"CreateWindowEx falhou", L"dx11-test", MB_OK | MB_ICONERROR);
    return 1;
  }

  if (!InitD3D(g_hwnd)) {
    ShutdownD3D();
    return 1;
  }

  ShowWindow(g_hwnd, show);
  UpdateWindow(g_hwnd);

  // Timer de alta resolucao para FPS + cor animada.
  LARGE_INTEGER freq{}, t0{}, t_prev{}, t_title{};
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&t0);
  t_prev = t0;
  t_title = t0;

  UINT frames_since_title = 0;
  char logbuf[128];

  MSG msg{};
  while (g_running) {
    // Drena mensagens sem bloquear (jogo real faz igual).
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) g_running = false;
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (!g_running) break;

    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    double elapsed =
        static_cast<double>(now.QuadPart - t0.QuadPart) / static_cast<double>(freq.QuadPart);

    // Cor animada: prova visual de que o frame e novo.
    float r = 0.5f + 0.5f * sinf(static_cast<float>(elapsed * 1.7));
    float g = 0.5f + 0.5f * sinf(static_cast<float>(elapsed * 2.3 + 2.1));
    float b = 0.5f + 0.5f * sinf(static_cast<float>(elapsed * 3.1 + 4.2));
    const float clear[4] = {r, g, b, 1.0f};
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_context->ClearRenderTargetView(g_rtv, clear);

    // VSYNC OFF: apresenta imediatamente, sem espera. E aqui que o
    // futuro hook de Present vai contar/medir/limitar.
    HRESULT hr = g_swapchain->Present(0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
      Log("Device lost — saindo. (Etapa A nao trata reset, apenas reporta.)");
      break;
    }

    frames_since_title++;

    double since_title =
        static_cast<double>(now.QuadPart - t_title.QuadPart) / static_cast<double>(freq.QuadPart);
    if (since_title >= 0.5) {
      double fps = frames_since_title / since_title;
      wchar_t title[128];
      _snwprintf_s(title, _countof(title), _TRUNCATE, L"dx11-test — FPS: %.0f (Present sem VSYNC)",
                   fps);
      SetWindowTextW(g_hwnd, title);
      // Tambem sai no DebugView / debugger — util na Etapa C/D para
      // comparar contagem interna vs. contagem do hook.
      snprintf(logbuf, sizeof(logbuf), "Present frames: %u em %.3fs = %.1f FPS",
               frames_since_title, since_title, fps);
      Log(logbuf);
      frames_since_title = 0;
      t_title = now;
    }

    (void)t_prev;
  }

  ShutdownD3D();
  DestroyWindow(g_hwnd);
  return 0;
}
