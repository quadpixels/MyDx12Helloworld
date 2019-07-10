#include <windows.h>
#include <stdio.h>

extern void InitDevice();
extern void InitCommandQ();
extern void InitSwapChain();
extern void EndInitializeFence();
extern void InitDescriptorHeap();
extern void Render();

extern bool g_init_done;

HWND g_hwnd;
int WIN_W = 1280, WIN_H = 720;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
  {
    LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
    SetWindowLongPtr(g_hwnd, GWLP_USERDATA, (LONG_PTR)pCreateStruct->lpCreateParams);
    break;
  }
  case WM_KEYDOWN:
    return 0;
  case WM_KEYUP:
    return 0;
  case WM_PAINT:
    Render();
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
  AllocConsole();
  freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
  freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

  // Create Window
  WNDCLASSEX wc = { 0 };
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"Step1";
  RegisterClassEx(&wc);

  RECT rect = { 0, 0, WIN_W, WIN_H };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  g_hwnd = CreateWindow(
    wc.lpszClassName,
    L"Step1",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    WIN_W, WIN_H,
    nullptr,
    nullptr,
    hInstance,
    nullptr
  );
  
  InitDevice();
  InitCommandQ();
  InitSwapChain();
  InitDescriptorHeap();
  //InitAssets();
  //InitTextureAndSRVs();
  //InitConstantBuffer();
  EndInitializeFence();

  printf("ShowWindow\n");
  ShowWindow(g_hwnd, nCmdShow);

  g_init_done = true;

  // Main message loop
  MSG msg = { 0 };
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  system("pause");
}