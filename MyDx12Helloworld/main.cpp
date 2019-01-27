#include "stdafx.h"
#include <DirectXMath.h>
#include <windows.h>
#include "WICTextureLoader12.h"
#include <vector>
#include <wincodec.h>
#include <wrl.h>
#include "Header.h"
#include <d3d11_1.h>

// Common for DX 11 and DX 12
using Microsoft::WRL::ComPtr;

const wchar_t* texNames[] = {
  L"pic2.png",
  L"pic1.png",
  L"pic3_small.png",
  L"projectile.png",
  L"brick.png",
  L"pic4.png",
  L"pic5.png",
  L"pic_exit.png",
};

bool IsWallTexture(int tid) {
  return (tid == 4);
}

int WIN_W = 1024, WIN_H = 640; 
HWND g_hwnd;
bool g_showBoundingBox = false;
const int NUM_TEXTURES = sizeof(texNames) / sizeof(texNames[0]);
const int NUM_SRVS = 2;
extern std::vector<SpriteInstance*> g_spriteInstances;

struct VertexUV {
  float x, y, z, u, v;
};
struct VertexUVNormal {
  float x, y, z, u, v, nx, ny, nz;
};
struct VertexAndColor {
  float x, y, z, r, g, b, a;
};
struct ConstantBufferData {
  ConstantBufferData() {
    x = y = w = h = win_w = win_h = 0.0f;
  }
  float x, y;
  float w, h; // width & height in pixels
  float win_w, win_h; // window w & h
  DirectX::XMMATRIX orientation;
};

PerSceneCBData g_per_scene_cb_data;
float g_cam_focus_x, g_cam_focus_y;

float g_cam_delta_x = 0;
float g_cam_delta_y = 0;
ConstantBufferData g_constantbufferdata;

struct ConstantBufferDataDrawLight {
  float WIN_W, WIN_H;
  float light_x, light_y, light_r;
  DirectX::XMVECTOR light_color;
  float global_alpha;
};
ConstantBufferDataDrawLight g_constantbufferdata_drawlight;

void CE(HRESULT x) {
  if (FAILED(x)) {
    printf("ERROR: %X\n", x);
    throw std::exception();
  }
}

unsigned char* LoadTexture(LPCWSTR file_name, UINT* w, UINT* h) {
  ComPtr<IWICImagingFactory> m_pWICFactory;
  HRESULT hr = S_OK;
  CE(CoInitialize(nullptr));
  CE(CoCreateInstance(
    CLSID_WICImagingFactory, nullptr,
    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWICFactory)));
  IWICImagingFactory* factory = m_pWICFactory.Get();

  IWICBitmapDecoder* decoder;
  IWICBitmapFrameDecode* bitmapSource = NULL;
  IWICFormatConverter* converter;
  factory->CreateDecoderFromFilename(
    file_name, // name of the file
    nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

  decoder->GetFrame(0, &bitmapSource);
  factory->CreateFormatConverter(&converter);
  converter->Initialize(
    bitmapSource, GUID_WICPixelFormat32bppPRGBA,
    WICBitmapDitherTypeNone, nullptr, 0.f,
    WICBitmapPaletteTypeMedianCut);

  bitmapSource->GetSize(w, h);
  printf("Bitmap size: %u x %u\n", *w, *h);
  const UINT size = 4 * (*w) * (*h);
  unsigned char* rgba = new unsigned char[size];
  CE(converter->CopyPixels(NULL, (*w) * 4, size, rgba)); // stride = sizeof(a row of image)
  return rgba;
}

// Shared between for DX 11 & 12
ID3DBlob* g_VS, *g_PS;
ID3DBlob* g_VS_boundingbox, *g_PS_boundingbox, *g_GS_boundingbox;
ID3DBlob* g_VS2, *g_PS2;
ID3DBlob *g_VS_combine, *g_PS_combine;
ID3DBlob *g_VS_drawlight, *g_PS_drawlight;
float g_scene_background[] = { 0.5f, 0.5f, 0.7f, 1.0f };

#include "dx11stuff.h"
#include "dx12stuff.h"

int DX_VER = 12;

// D3D12

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
  AllocConsole();
  freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
  freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

  printf("_WIN32 = %X\n", _WIN32);
  printf("_WIN32_WINNT = %X\n", _WIN32_WINNT);

  // Create Window
  WNDCLASSEX wc = { 0 };
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  if (DX_VER == 12) wc.lpszClassName = L"My DX 12 Helloworld";
  else wc.lpszClassName = L"My DX 11 Helloworld";
  RegisterClassEx(&wc);

  RECT rect = { 0, 0, WIN_W, WIN_H };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  g_hwnd = CreateWindow(
    wc.lpszClassName,
    wc.lpszClassName, // Window Name
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    WIN_W, WIN_H,
    nullptr,
    nullptr,
    hInstance,
    nullptr
  );

  if (DX_VER == 12) {

    // INITIALIZE
    InitDevice();
    InitCommandQ();
    InitSwapChain();
    InitDescriptorHeap();
    InitAssets();
    InitTextureAndSRVs();
    InitConstantBuffer();

    EndInitializeFence();
  }
  else { // DX 11
    InitDevice11();
    InitAssets11();
    InitTextureAndSRVs11();
    InitConstantBuffer11();
  }
  
  {
    // Proj matrix
    float aspect_ratio = WIN_W * 1.0f / WIN_H;
    float fovy = 45.0f / (180.0f / 3.14159f);
    g_per_scene_cb_data.projection = DirectX::XMMatrixPerspectiveFovLH(fovy, aspect_ratio, 0.01f, 0.2f);
  }

  g_init_done = true;

  // GAMEPLAY!
  LoadDummyAssets();
  LoadLevel(0);

  ShowWindow(g_hwnd, nCmdShow);

  // Main message loop
  MSG msg = { 0 };
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}

void Update() {
  if (DX_VER == 12)
    Update_DX12();
  else
    Update_DX11();
}

void Render() {
  if (g_init_done == false) return;

  if (DX_VER == 12)
    Render_DX12();
  else
    Render_DX11();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
  {
    LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
    SetWindowLongPtr(g_hwnd, GWLP_USERDATA, (LONG_PTR)pCreateStruct->lpCreateParams);
    break;
  }
  case WM_KEYDOWN:
    OnKeyDown(wParam, lParam);
    return 0;
  case WM_KEYUP:
    OnKeyUp(wParam, lParam);
    return 0;
  case WM_PAINT:
    GameplayUpdate();
    Update();
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