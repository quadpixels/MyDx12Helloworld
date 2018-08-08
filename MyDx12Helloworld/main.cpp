#include "stdafx.h"
#include <DirectXMath.h>
#include <windows.h>
#include "WICTextureLoader12.h"
#include <vector>
#include <wincodec.h>
#include <wrl.h>
#include "Header.h"

bool g_showBoundingBox = false;

using Microsoft::WRL::ComPtr;
const int NUM_OBJECTS = 256; // CB size is at most 64KB ?!
int WIN_W = 1024, WIN_H = 640;
const int FrameCount = 2;
const int TexturePixelSize = 4;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HWND g_hwnd;
ID3D12Device* g_device;
ID3D12CommandQueue* g_commandqueue;
IDXGISwapChain3* g_swapchain;
IDXGIFactory4* g_factory;
ID3D12DescriptorHeap* g_descriptorheap;
ID3D12Resource* g_rendertargets[FrameCount];
ID3D12CommandAllocator* g_commandallocator; // 1 allocator for <= 1 "recording" cmd lists
ID3D12CommandAllocator* g_commandallocator1;
ID3D12RootSignature* g_rootsignature;
ID3D12RootSignature* g_rootsignature1;
ID3D12PipelineState* g_pipelinestate;
ID3D12PipelineState* g_pipelinestate1;
ID3D12GraphicsCommandList* g_commandlist;
ID3D12GraphicsCommandList* g_commandlist1;
ID3D12Fence* g_fence;
int g_frameindex, g_rtvDescriptorSize, g_fencevalue;
HANDLE g_fenceevent;
CD3DX12_VIEWPORT g_viewport;
CD3DX12_RECT g_scissorrect;

ID3DBlob* g_VS, *g_PS;
ID3DBlob* g_VS1, *g_PS1, *g_GS1;
ID3D12Resource* g_vertexbuffer;
ID3D12Resource* g_vertexbuffer1;
D3D12_VERTEX_BUFFER_VIEW g_vertexbufferview;
D3D12_VERTEX_BUFFER_VIEW g_vertexbufferview1;
ID3D12Resource* g_texture;
ID3D12DescriptorHeap* g_srvheap;
ID3D12Resource* g_constantbuffer;
unsigned char* g_pCbvDataBegin;
int g_cbvDescriptorSize;

bool g_init_done = false;

const wchar_t* texNames[] = { 
  L"pic2.png",
  L"pic1.png", 
  L"pic3_small.png", 
  L"projectile.png",
  L"brick.png",
};
const int NUM_TEXTURES = sizeof(texNames) / sizeof(texNames[0]);

extern std::vector<SpriteInstance*> g_spriteInstances;

struct VertexAndUV {
  float x, y, z, u, v;
};
struct VertexAndColor {
  float x, y, z, r, g, b, a;
};

PerSceneCBData g_per_scene_cb_data;

struct ConstantBufferData {
  ConstantBufferData() {
    x = y = w = h = win_w = win_h = 0.0f;
  }
  float x, y;
  float w, h; // width & height in pixels
  float win_w, win_h; // window w & h
  DirectX::XMMATRIX orientation;
};

float g_cam_delta_x = 0;
float g_cam_delta_y = 0;
ConstantBufferData g_constantbufferdata;

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
  CE(converter->CopyPixels(NULL, (*w)*4, size, rgba)); // stride = sizeof(a row of image)
  return rgba;
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
}

void InitDescriptorHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC desc = {};
  desc.NumDescriptors = FrameCount;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  CE(g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_descriptorheap)));

  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = NUM_TEXTURES + 1; // +1 for the constant buffer
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  CE(g_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_srvheap)));

  g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  g_cbvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_descriptorheap->GetCPUDescriptorHandleForHeapStart());
  for (UINT i = 0; i < FrameCount; i++) {
    CE(g_swapchain->GetBuffer(i, IID_PPV_ARGS(&g_rendertargets[i])));
    g_device->CreateRenderTargetView(g_rendertargets[i], nullptr, rtvHandle);
    rtvHandle.Offset(1, g_rtvDescriptorSize);
  }

  CE(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandallocator)));
  CE(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandallocator1)));
}

void InitAssets() {
  {
    D3D12_ROOT_DESCRIPTOR rootCbvDescriptor, rootSrvDescriptor;
    rootCbvDescriptor.RegisterSpace = 0;
    rootCbvDescriptor.ShaderRegister = 0;

    rootSrvDescriptor.RegisterSpace = 0;
    rootSrvDescriptor.ShaderRegister = 0;

    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // We have only 1 table: { SRV, SRV, CBV }
//    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsConstantBufferView(0U, 0U, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[2].InitAsConstantBufferView(1U, 0U, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
//    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    rootParameters[1].Descriptor = rootCbvDescriptor;
//    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 4;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature, error;
    HRESULT x = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, 
      D3D_ROOT_SIGNATURE_VERSION_1_1,
      &signature,
      &error);

    if (signature == nullptr) {
      printf("ERROR:%s\n", (char*)(error->GetBufferPointer()));
    }

    CE(g_device->CreateRootSignature(0, signature->GetBufferPointer(), 
      signature->GetBufferSize(), IID_PPV_ARGS(&g_rootsignature)));
    g_rootsignature->SetName(L"g_rootsignature");
  }

#if defined(_DEBUG)
  // Enable better shader debugging with the graphics debugging tools.
  UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  UINT compileFlags = 0;
#endif

  ID3DBlob* error = nullptr;
  CE(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS, &error));
  if (error)
    printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS, &error));
  if (error)
    printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());

  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS1, &error));
  if (error)
    printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS1, &error));
  if (error)
    printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "GSMain", "gs_5_0", compileFlags, 0, &g_GS1, &error));
  if (error)
    printf("Error compiling GS: %s\n", (char*)error->GetBufferPointer());

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } // May cause error in CreateGraphicsPipelineState
  };

  // PSO #0

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
  psoDesc.pRootSignature = g_rootsignature;
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_VS);
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_PS);
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.DepthClipEnable = FALSE;
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthEnable = FALSE;
  psoDesc.DepthStencilState.StencilEnable = FALSE;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  
  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate)));

  CE(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandallocator, g_pipelinestate, IID_PPV_ARGS(&g_commandlist)));

  // PSO #1
  D3D12_INPUT_ELEMENT_DESC inputElementDescs1[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } // May cause error in CreateGraphicsPipelineState
  };

  psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_VS1);
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_PS1);
  psoDesc.GS = CD3DX12_SHADER_BYTECODE(g_GS1);
  psoDesc.InputLayout = { inputElementDescs1, _countof(inputElementDescs1) };
  psoDesc.RasterizerState.AntialiasedLineEnable = true;
  //psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  
  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate1)));
  CE(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandallocator1, g_pipelinestate1, IID_PPV_ARGS(&g_commandlist1)));

  // Create vertex buffer & view
  {
    VertexAndUV verts[] = {
      {  0.5f,  0.5f, 0.0f,  1.0f, 0.0f }, //  
      {  0.5f, -0.5f, 0.0f,  1.0f, 1.0f }, //  +-----------+ UV = (1, 0), NDC=(1, 1)
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f }, //  |           |
                                           //  |           |
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f }, //  |           |
      { -0.5f,  0.5f, 0.0f,  0.0f, 0.0f }, //  |           |
      {  0.5f,  0.5f, 0.0f,  1.0f, 0.0f }, //  +-----------+ UV = (1, 1), NDC=(1,-1)
    };
    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(verts)),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&g_vertexbuffer)
    ));

    // Upload
    UINT8* pData;
    CD3DX12_RANGE readRange(0, 0);
    CE(g_vertexbuffer->Map(0, &readRange, (void**)(&pData)));
    memcpy(pData, verts, sizeof(verts));
    g_vertexbuffer->Unmap(0, nullptr);

    g_vertexbufferview.BufferLocation = g_vertexbuffer->GetGPUVirtualAddress();
    g_vertexbufferview.StrideInBytes = sizeof(VertexAndUV);
    g_vertexbufferview.SizeInBytes = sizeof(verts);
  }

  {
    const float EPS = 0.007f;
    VertexAndColor verts[] = { // Treat as trianglez
      { 0.5f,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f },     //     -+ ср
      { 0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },    //     \|
      { 0.5f-EPS,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f }, //      |

      { 0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },    //
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },   //       __-  об
      { 0.5f, -0.5f+EPS, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },//   ------+

      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },    //   |
      { -0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },    //   |\   вС
      { -0.5f+EPS, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },//   +-

      { -0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },    //
      { 0.5f,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f },      //   +------   ио
      { -0.5f,  0.5f-EPS, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },//   ---
    };
    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(verts)),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&g_vertexbuffer1)
    ));

    // Upload
    UINT8* pData;
    CD3DX12_RANGE readRange(0, 0);
    CE(g_vertexbuffer1->Map(0, &readRange, (void**)(&pData)));
    memcpy(pData, verts, sizeof(verts));
    g_vertexbuffer1->Unmap(0, nullptr);

    g_vertexbufferview1.BufferLocation = g_vertexbuffer1->GetGPUVirtualAddress();
    g_vertexbufferview1.StrideInBytes = sizeof(VertexAndColor);
    g_vertexbufferview1.SizeInBytes = sizeof(verts);
  }
}

void InitTexture() {

  CD3DX12_CPU_DESCRIPTOR_HANDLE h(g_srvheap->GetCPUDescriptorHandleForHeapStart());

  for (int i = 0; i < NUM_TEXTURES; i++) {
    ID3D12Resource* texture;

    UINT tex_w, tex_h;
    unsigned char* rgba = LoadTexture(LPCWSTR(texNames[i]), &tex_w, &tex_h);
    printf("Texture size: %u x %u\n", tex_w, tex_h);

    ID3D12Resource* textureUploadHeap;

    D3D12_RESOURCE_DESC textureDesc = {};
    // Describe and create a Texture2D.
    textureDesc.MipLevels = 4;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = tex_w;
    textureDesc.Height = tex_h;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // Create the texture.
    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&texture)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture, 0, 1) + D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    // Create the GPU upload buffer.
    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&textureUploadHeap)));

    // Copy data to the intermediate upload heap and then schedule a copy 
    // from the upload heap to the Texture2D.
    //std::vector<UINT8> texture = GenerateTextureData();

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = rgba;
    textureData.RowPitch = tex_w * TexturePixelSize;
    textureData.SlicePitch = textureData.RowPitch * tex_h;

    UpdateSubresources<1>(g_commandlist, texture, textureUploadHeap, 0, 0, 1, &textureData);
    g_commandlist->ResourceBarrier(1,
      &CD3DX12_RESOURCE_BARRIER::Transition(texture,
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // Describe and create a SRV for the texture.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    g_device->CreateShaderResourceView(texture, 
      &srvDesc, 
      h);
    h.Offset(g_cbvDescriptorSize);
  }
}

void InitConstantBuffer() {
  // CBV
  CE(g_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&g_constantbuffer)));

  const unsigned SIZE = 256 * (NUM_OBJECTS - NUM_TEXTURES); // Assume we have 100 objects
  // Describe and create a constant buffer view.
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
  cbvDesc.BufferLocation = g_constantbuffer->GetGPUVirtualAddress();
  cbvDesc.SizeInBytes = SIZE;

  CD3DX12_CPU_DESCRIPTOR_HANDLE handle1(g_srvheap->GetCPUDescriptorHandleForHeapStart(), NUM_TEXTURES, g_cbvDescriptorSize);

  g_device->CreateConstantBufferView(&cbvDesc, handle1);


  // Map and initialize the constant buffer. We don't unmap this until the
  // app closes. Keeping things mapped for the lifetime of the resource is okay.
  CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
  CE(g_constantbuffer->Map(0, &readRange, (void**)&g_pCbvDataBegin));
  memcpy(g_pCbvDataBegin, &g_constantbufferdata, sizeof(g_constantbufferdata));
  g_constantbuffer->Unmap(0, NULL);
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
  CE(g_commandlist1->Close());
  ID3D12CommandList* ppCommandLists[] = { g_commandlist };
  g_commandqueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
  CE(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
  g_fencevalue = 1;
  g_fenceevent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  WaitForPreviousFrame();
}

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
  wc.lpszClassName = L"My DX 12 Helloworld";
  RegisterClassEx(&wc);

  RECT rect = { 0, 0, WIN_W, WIN_H };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  g_hwnd = CreateWindow(
    wc.lpszClassName,
    L"My DX 12 Helloworld",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    WIN_W, WIN_H,
    nullptr,
    nullptr,
    hInstance,
    nullptr
  );

  // INITIALIZE
  InitDevice();
  InitCommandQ();
  InitSwapChain();
  InitDescriptorHeap();
  InitAssets();
  InitTexture();
  InitConstantBuffer();
  EndInitializeFence();
  
  {
    // Proj matrix
    float aspect_ratio = WIN_W * 1.0 / WIN_H;
    float fovy = 45.0f / (180.0f / 3.14159f);
    g_per_scene_cb_data.projection = DirectX::XMMatrixPerspectiveFovLH(fovy, aspect_ratio, 0.1f, 100.0f);
  }

  g_init_done = true;

  // GAMEPLAY!
  PopulateDummy();

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

  const float translationSpeed = 2.0f; // Pixel Per Frame
  const float offsetBounds = 1.25f;

  const size_t N = g_spriteInstances.size();
  const UINT ALIGN = 256;

  CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
  CE(g_constantbuffer->Map(0, &readRange, (void**)&g_pCbvDataBegin));

  // CB layout:
  // [0] = scene projection & view matrix
  // [1..N] = obj-wise locations
  memcpy(g_pCbvDataBegin, &g_per_scene_cb_data, sizeof(g_per_scene_cb_data));

  for (int i = 0; i < N; i++) {
    SpriteInstance* sInst = g_spriteInstances.at(i);
    ConstantBufferData d;
    d.x = sInst->x;
    d.y = sInst->y;
    d.w = sInst->w;
    d.h = sInst->h;
    d.win_h = WIN_H;
    d.win_w = WIN_W;
    d.orientation = sInst->orientation;
    memcpy(g_pCbvDataBegin + (1+i)*ALIGN, &d, sizeof(d));
  }

  g_constantbuffer->Unmap(0, NULL);
}

void Render() {
  if (g_init_done == false) return;

  // Populate Command List
  CE(g_commandallocator->Reset());
  CE(g_commandlist->Reset(g_commandallocator, g_pipelinestate));
  CE(g_commandallocator1->Reset());
  CE(g_commandlist1->Reset(g_commandallocator1, g_pipelinestate1));

  g_commandlist->SetGraphicsRootSignature(g_rootsignature);
  g_commandlist1->SetGraphicsRootSignature(g_rootsignature);

  // Bind resources

  ID3D12DescriptorHeap* ppHeaps[] = { g_srvheap };
  g_commandlist->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);  
  g_commandlist1->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  
  g_commandlist->RSSetViewports(1, &g_viewport);
  g_commandlist->RSSetScissorRects(1, &g_scissorrect);
  g_commandlist1->RSSetViewports(1, &g_viewport);
  g_commandlist1->RSSetScissorRects(1, &g_scissorrect);

  // Draw stuff

  g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
  g_commandlist1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_descriptorheap->GetCPUDescriptorHandleForHeapStart(), g_frameindex, g_rtvDescriptorSize);
  const float clearColor[] = { 1.0f, 1.0f, 0.8f, 1.0f };

  g_commandlist->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
  g_commandlist1->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

  g_commandlist->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  g_commandlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_commandlist->IASetVertexBuffers(0, 1, &g_vertexbufferview);

  g_commandlist1->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_commandlist1->IASetVertexBuffers(0, 1, &g_vertexbufferview1);

  g_commandlist->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress()); // the CB is in slot 1 of the root signature...
  
  //g_commandlist->SetGraphicsRootDescriptorTable(0, gpuSrvHandle);
  //g_commandlist->DrawInstanced(6, 1, 0, 0);

  CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle;

  if (g_showBoundingBox) {
    // Make debug layer happy
    gpuSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_srvheap->GetGPUDescriptorHandleForHeapStart());
    g_commandlist1->SetGraphicsRootDescriptorTable(0, gpuSrvHandle);
    g_commandlist1->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress());
    g_commandlist1->SetGraphicsRootConstantBufferView(2, g_constantbuffer->GetGPUVirtualAddress());

    g_commandlist1->DrawInstanced(12, 1, 0, 0);
  }

  const size_t N = g_spriteInstances.size();
  const UINT ALIGN = 256;

  g_commandlist->SetGraphicsRootConstantBufferView(2, g_constantbuffer->GetGPUVirtualAddress());

  for (int i = 0; i < N; i++) {
    SpriteInstance* pSprInst = g_spriteInstances[i];
    const int textureId = pSprInst->pSprite->textureId;

    gpuSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_srvheap->GetGPUDescriptorHandleForHeapStart());
    gpuSrvHandle.Offset(textureId * g_cbvDescriptorSize);

    g_commandlist->SetGraphicsRootDescriptorTable(0, gpuSrvHandle);
    g_commandlist->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress() + ALIGN * (1+i)); // the CB is in slot 1 of the root signature...
    g_commandlist->DrawInstanced(6, 1, 0, 0);

    if (g_showBoundingBox) {
      g_commandlist1->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress() + ALIGN * (1+i)); // the CB is in slot 1 of the root signature...
      g_commandlist1->DrawInstanced(12, 1, 0, 0);
    }
  }

  g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
  g_commandlist1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
  CE(g_commandlist->Close());
  CE(g_commandlist1->Close());

  // Execute Command List
  g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist));
  g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist1));

  // Flip the swapchain
  CE(g_swapchain->Present(1, 0));

  WaitForPreviousFrame();
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