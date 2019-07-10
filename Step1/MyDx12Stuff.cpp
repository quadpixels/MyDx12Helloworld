#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <Windows.h>
#include <wrl.h>
#include <vector>
#include "d3dx12.h"

extern int WIN_W, WIN_H;
extern HWND g_hwnd;

IDXGIFactory4* g_factory;
ID3D12Device* g_device;
CD3DX12_VIEWPORT g_viewport;
CD3DX12_RECT g_scissorrect;
HANDLE g_fenceevent;

ID3D12CommandQueue* g_commandqueue;
IDXGISwapChain3* g_swapchain;
const int FrameCount = 2;
int g_frameindex = 0, g_fencevalue = 0;

ID3D12GraphicsCommandList* g_commandlist;
ID3D12Fence* g_fence;
ID3D12CommandAllocator* g_commandallocator;
ID3D12DescriptorHeap* g_descriptorheap;
ID3D12Resource* g_rendertargets[FrameCount];
int g_rtvDescriptorSize;

bool g_init_done = false;

void CE(HRESULT x) {
  if (FAILED(x)) {
    printf("ERROR: %X\n", x);
    throw std::exception();
  }
}

void InitDevice() {
  UINT dxgiFactoryFlags = 0;
  bool useWarpDevice = false;

  ID3D12Debug* debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    printf("Enabling debug layer\n");
  }

  CE(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&g_factory)));
  if (useWarpDevice) {
    IDXGIAdapter* warpAdapter;
    CE(g_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    CE(D3D12CreateDevice(warpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));

    printf("Created a WARP device = %p\n", g_device);
  }
  else {
    IDXGIAdapter1* hwAdapter;
    for (UINT idx = 0; g_factory->EnumAdapters1(idx, &hwAdapter) != DXGI_ERROR_NOT_FOUND; idx++) {
      DXGI_ADAPTER_DESC1 desc;
      hwAdapter->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
      else {
        CE(D3D12CreateDevice(hwAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));
      }
    }
    printf("Created a hardware device = %p\n", g_device);
  }

  g_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, WIN_W*1.0f, WIN_H*1.0f, -100.0f, 100.0f);
  g_scissorrect = CD3DX12_RECT(0, 0, long(WIN_W), long(WIN_H));
}

void InitCommandQ() {
  D3D12_COMMAND_QUEUE_DESC desc = { };
  desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  CE(g_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_commandqueue)));
  CE(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandallocator)));

  CE(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandallocator, nullptr, IID_PPV_ARGS(&g_commandlist)));
}

void InitSwapChain() {
  DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
  desc.BufferCount = FrameCount;
  desc.Width = WIN_W; desc.Height = WIN_H;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  desc.SampleDesc.Count = 1;
  IDXGISwapChain1* swapchain;
  CE(g_factory->CreateSwapChainForHwnd(g_commandqueue, g_hwnd, &desc, nullptr, nullptr, &swapchain));
  g_swapchain = (IDXGISwapChain3*)swapchain;
  CE(g_factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));
  g_frameindex = g_swapchain->GetCurrentBackBufferIndex();
  printf("[InitSwapChain] g_frameindex=%d\n", g_frameindex);
}

void InitDescriptorHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC desc = {};
  desc.NumDescriptors = FrameCount; // [2 x SwapChainTarget]
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  CE(g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_descriptorheap)));

  g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // Create RTV
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_descriptorheap->GetCPUDescriptorHandleForHeapStart());
  for (unsigned i = 0; i < FrameCount; i++) {
    CE(g_swapchain->GetBuffer(i, IID_PPV_ARGS(&g_rendertargets[i])));
    g_device->CreateRenderTargetView(g_rendertargets[i], nullptr, rtvHandle);
    rtvHandle.Offset(1, g_rtvDescriptorSize);
  }

  g_rendertargets[0]->SetName(L"Rendertarget #0");
  g_rendertargets[1]->SetName(L"Rendertarget #1");
}

void WaitForPreviousFrame() {
  const UINT fence = g_fencevalue;
  CE(g_commandqueue->Signal(g_fence, fence));
  g_fencevalue++;

  if (g_fence->GetCompletedValue() < fence) {
    CE(g_fence->SetEventOnCompletion(fence, g_fenceevent));
    CE(WaitForSingleObject(g_fenceevent, INFINITE));
  }

  g_frameindex = g_swapchain->GetCurrentBackBufferIndex();
}

void EndInitializeFence() {
  CE(g_commandlist->Close());
  ID3D12CommandList* ppCommandLists[] = { g_commandlist };
  g_commandqueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
  CE(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
  g_fencevalue = 1;
  g_fenceevent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  WaitForPreviousFrame();
}

void Render() {
  if (g_init_done == false) return;

  CE(g_commandallocator->Reset());
  CE(g_commandlist->Reset(g_commandallocator, nullptr));

  g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(g_descriptorheap->GetCPUDescriptorHandleForHeapStart(), g_frameindex * g_rtvDescriptorSize);
  float cc[] = { 1.0f, 1.0f, 0.7f, 1.0f };
  g_commandlist->ClearRenderTargetView(cpu_handle, cc, 0, nullptr);

  g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

  CE(g_commandlist->Close());

  g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist));

  CE(g_swapchain->Present(1, 0));

  // If there is no this call there will be an error at g_commandallocator
  WaitForPreviousFrame();
}