const int NUM_OBJECTS = 256; // CB size is at most 64KB ?!
const int FrameCount = 2;
const int TexturePixelSize = 4;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ID3D12Device* g_device;
ID3D12CommandQueue* g_commandqueue;
IDXGISwapChain3* g_swapchain;
IDXGIFactory4* g_factory;
ID3D12DescriptorHeap* g_descriptorheap;
ID3D12Resource* g_rendertargets[FrameCount];
ID3D12Resource* g_maincanvas;
ID3D12Resource* g_scatterlight;

ID3D12CommandAllocator* g_commandallocator; // 1 allocator for <= 1 "recording" cmd lists
ID3D12CommandAllocator* g_commandallocator_bb;
ID3D12CommandAllocator* g_commandallocator_copier;
ID3D12RootSignature* g_rootsignature;
ID3D12RootSignature* g_rootsignature_bb;
ID3D12RootSignature* g_rootsignature_copier;
ID3D12RootSignature* g_rootsignature_combine;
ID3D12RootSignature* g_rootsignature_drawlight; // start adding drawlight: 20180826 20:41
ID3D12PipelineState* g_pipelinestate;
ID3D12PipelineState* g_pipelinestate_bb;
ID3D12PipelineState* g_pipelinestate_copier;
ID3D12PipelineState* g_pipelinestate_drawlight;
ID3D12PipelineState* g_pipelinestate_lightmask;
ID3D12PipelineState* g_pipelinestate_combine;

ID3D12GraphicsCommandList* g_commandlist;
ID3D12GraphicsCommandList* g_commandlist_bb;
ID3D12GraphicsCommandList* g_commandlist_copier;
ID3D12Fence* g_fence;
int g_frameindex, g_rtvDescriptorSize, g_fencevalue;
HANDLE g_fenceevent;
CD3DX12_VIEWPORT g_viewport;
CD3DX12_RECT g_scissorrect;

ID3D12Resource* g_vb_unitsquare;
ID3D12Resource* g_vb_unitcube;
ID3D12Resource* g_vertexbuffer1;
ID3D12Resource* g_vb_fsquad; // FS Quad = Full Screen Quad

D3D12_VERTEX_BUFFER_VIEW g_vbv_unitsquare;
D3D12_VERTEX_BUFFER_VIEW g_vbv_unitcube;
D3D12_VERTEX_BUFFER_VIEW g_vbv_fsquad;
D3D12_VERTEX_BUFFER_VIEW g_vertexbufferview1;
ID3D12Resource* g_texture;
ID3D12DescriptorHeap* g_srvheap;
ID3D12Resource* g_constantbuffer;
ID3D12Resource* g_constantbuffer_drawlight;
unsigned char* g_pCbvDataBegin;
int g_cbvDescriptorSize;

ID3D12Resource* g_dsv_buffer;
ID3D12DescriptorHeap* g_dsv_heap;
int g_dsvDescriptorSize;

bool g_init_done = false;


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
  // Why are there 2 descriptors? reason:
  // [ 2xSwapChainTarget ] [ scatter canvas ] [ scatter light ]
  desc.NumDescriptors = FrameCount + 2;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  CE(g_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_descriptorheap)));

  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = NUM_TEXTURES + 4; // +1 for the constant buffer, +2 for the two SRVs, +1 for the light parameters
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  CE(g_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_srvheap)));

  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  CE(g_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_dsv_heap)));

  g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  g_cbvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  g_dsvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_descriptorheap->GetCPUDescriptorHandleForHeapStart());
  for (UINT i = 0; i < FrameCount; i++) {
    CE(g_swapchain->GetBuffer(i, IID_PPV_ARGS(&g_rendertargets[i])));
    g_device->CreateRenderTargetView(g_rendertargets[i], nullptr, rtvHandle);
    rtvHandle.Offset(1, g_rtvDescriptorSize);
  }
  g_rendertargets[0]->SetName(L"Render Target Frame 0");
  g_rendertargets[1]->SetName(L"Render Target Frame 1");

  // create the rt for light scatter (main canvas & lights)
  D3D12_RESOURCE_DESC textureDesc = {};
  // Describe and create a Texture2D.
  textureDesc.MipLevels = 1;
  textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  textureDesc.Width = WIN_W;
  textureDesc.Height = WIN_H;
  textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  textureDesc.DepthOrArraySize = 1;
  textureDesc.SampleDesc.Count = 1;
  textureDesc.SampleDesc.Quality = 0;
  textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

  {
    CD3DX12_CLEAR_VALUE clear_value(DXGI_FORMAT_R8G8B8A8_UNORM, g_scene_background);

    HRESULT result = g_device->CreateCommittedResource(
      &(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      &clear_value,
      IID_PPV_ARGS(&g_maincanvas)
    );
    g_maincanvas->SetName(L"Main canvas");
    CE(result);
    g_device->CreateRenderTargetView(g_maincanvas, nullptr, rtvHandle);
  }

  {
    rtvHandle.Offset(1, g_rtvDescriptorSize);
    float zeros[] = { .0f, .0f, .0f, .0f };
    CD3DX12_CLEAR_VALUE cv1(DXGI_FORMAT_R8G8B8A8_UNORM, zeros);
    HRESULT result = g_device->CreateCommittedResource(
      &(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      &cv1,
      IID_PPV_ARGS(&g_scatterlight)
    );
    g_scatterlight->SetName(L"Light map");
    CE(result);
    g_device->CreateRenderTargetView(g_scatterlight, nullptr, rtvHandle);
  }

  {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = { };
    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, WIN_W, WIN_H, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      &depthOptimizedClearValue,
      IID_PPV_ARGS(&g_dsv_buffer)
    );
    g_device->CreateDepthStencilView(g_dsv_buffer, &dsv_desc, g_dsv_heap->GetCPUDescriptorHandleForHeapStart());
  }

  CE(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandallocator)));
  CE(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandallocator_bb)));
  CE(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandallocator_copier)));
}


void InitAssets() {
  {
    D3D12_ROOT_DESCRIPTOR rootCbvDescriptor, rootSrvDescriptor;
    rootCbvDescriptor.RegisterSpace = 0;
    rootCbvDescriptor.ShaderRegister = 0;

    rootSrvDescriptor.RegisterSpace = 0;
    rootSrvDescriptor.ShaderRegister = 0;

    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // We have only 1 table: { SRV, SRV, CBV }

    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsConstantBufferView(0U, 0U, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[2].InitAsConstantBufferView(1U, 0U, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 4;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Main signature
    {
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

    // Combiner signature
    {
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
      rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[1].InitAsConstantBufferView(0U, 0U, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);

      CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
      rootSignatureDesc.Init_1_1(2, rootParameters,
        1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
      );
      ID3DBlob *signature, *error;
      HRESULT x = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature, &error);
      if (signature == nullptr) {
        printf("Error create root signature for combine: %s\n", (char*)(error->GetBufferPointer()));
      }
      CE(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&g_rootsignature_combine)));
      g_rootsignature_combine->SetName(L"g_rootsignature_combine");
    }

    // DrawLight signature
    {
      rootParameters[0].InitAsConstantBufferView(0U, 0U, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        D3D12_SHADER_VISIBILITY_ALL);
      CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
      rootSignatureDesc.Init_1_1(1, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
      ID3DBlob *signature, *error;
      HRESULT x = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature, &error);
      if (signature == nullptr) {
        printf("Error create root signature for combine: %s\n", (char*)(error->GetBufferPointer()));
      }
      CE(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&g_rootsignature_drawlight)));
      g_rootsignature_drawlight->SetName(L"g_rootsignature_drawlight");
    }
  }

#if defined(_DEBUG)
  // Enable better shader debugging with the graphics debugging tools.
  UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  UINT compileFlags = 0;
#endif

  ID3DBlob* error = nullptr;
  CE(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());

  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS1, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS1, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "GSMain", "gs_5_0", compileFlags, 0, &g_GS1, &error));
  if (error) printf("Error compiling GS: %s\n", (char*)error->GetBufferPointer());

  CE(D3DCompileFromFile(L"shaders_mask.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS2, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shaders_mask.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS2, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());

  CE(D3DCompileFromFile(L"shaders_combine.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS_combine, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shaders_combine.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS_combine, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());

  CE(D3DCompileFromFile(L"shaders_drawlight.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS_drawlight, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shaders_drawlight.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS_drawlight, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());

  D3D12_INPUT_ELEMENT_DESC inputElementDescsWithNormal[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // May cause error in CreateGraphicsPipelineState
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  D3D12_INPUT_ELEMENT_DESC inputElementDescsNoNormal[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // May cause error in CreateGraphicsPipelineState
  };

  // PSO #0

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputElementDescsWithNormal, _countof(inputElementDescsWithNormal) };
  psoDesc.pRootSignature = g_rootsignature;
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_VS);
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_PS);
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.DepthClipEnable = FALSE;

  psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;

  D3D12_RENDER_TARGET_BLEND_DESC rtbd;
  ZeroMemory(&rtbd, sizeof(rtbd));
  rtbd.BlendEnable = true;
  rtbd.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  rtbd.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  rtbd.BlendOp = D3D12_BLEND_OP_ADD;
  rtbd.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  rtbd.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  rtbd.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  rtbd.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  psoDesc.BlendState.RenderTarget[0] = rtbd;

  psoDesc.DepthStencilState.DepthEnable = TRUE;
  psoDesc.DepthStencilState.StencilEnable = FALSE;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate)));
  CE(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandallocator, g_pipelinestate, IID_PPV_ARGS(&g_commandlist)));


  psoDesc.DepthStencilState.DepthEnable = FALSE;

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


  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate_copier)));
  CE(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandallocator_copier, g_pipelinestate_copier, IID_PPV_ARGS(&g_commandlist_copier)));

  // PSO #2 is the same as #1
  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate_bb)));
  CE(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandallocator_bb, g_pipelinestate_bb, IID_PPV_ARGS(&g_commandlist_bb)));

  psoDesc.DepthStencilState.DepthEnable = FALSE;;

  // Disable blending for light mask and combine
  rtbd.BlendEnable = false;
  psoDesc.BlendState.RenderTarget[0] = rtbd;
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_VS2);
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_PS2);
  psoDesc.GS.BytecodeLength = 0;
  psoDesc.GS.pShaderBytecode = nullptr;
  psoDesc.InputLayout = { inputElementDescsNoNormal, _countof(inputElementDescsNoNormal) };
  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate_lightmask)));

  psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_VS_combine);
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_PS_combine);
  psoDesc.pRootSignature = g_rootsignature_combine;
  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate_combine)));

  psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_VS_drawlight);
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_PS_drawlight);
  psoDesc.pRootSignature = g_rootsignature_drawlight;
  CE(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelinestate_drawlight)));

  // Create vertex buffer & view for unit square
  // Normal is set to ZERO b/c these are DOUBLE-SIDED for now
  {
    VertexUVNormal verts[] = {
      {  0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f }, //  
      {  0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f, 0.0f }, //  +-----------+ UV = (1, 0), NDC=(1, 1)
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 0.0f }, //  |           |
                                                             //  |           |
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 0.0f }, //  |           |
      { -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, //  |           |
      {  0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f }, //  +-----------+ UV = (1, 1), NDC=(1,-1)
    };
    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(verts)),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&g_vb_unitsquare)
    ));

    // Upload
    UINT8* pData;
    CD3DX12_RANGE readRange(0, 0);
    CE(g_vb_unitsquare->Map(0, &readRange, (void**)(&pData)));
    memcpy(pData, verts, sizeof(verts));
    g_vb_unitsquare->Unmap(0, nullptr);

    g_vbv_unitsquare.BufferLocation = g_vb_unitsquare->GetGPUVirtualAddress();
    g_vbv_unitsquare.StrideInBytes = sizeof(VertexUVNormal);
    g_vbv_unitsquare.SizeInBytes = sizeof(verts);
  }

  // Create vertex buffer & view for unit cube
  {
    VertexUVNormal verts[] = {
      // -Z
      {  0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 0.0f, -1.0f }, //  
      {  0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 0.0f, -1.0f }, //  +-----------+ UV = (1, 0), NDC=(1, 1)
      { -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 0.0f, -1.0f }, //  |           |
                                            //  |    -Z     |
      { -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 0.0f, -1.0f }, //  |           |
      { -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 0.0f, 0.0f, -1.0f }, //  |           |
      {  0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 0.0f, -1.0f }, //  +-----------+ UV = (1, 1), NDC=(1,-1)

      // +X
      { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
      { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
      { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
      { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
      { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
      { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },

      // -X
      { -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f },
      { -0.5f, -0.5f, -0.5f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f },
      { -0.5f, -0.5f,  0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f },
      { -0.5f, -0.5f,  0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f },
      { -0.5f,  0.5f,  0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f },
      { -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f },

      // +Y
      {  0.5f, 0.5f,  0.5f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f },
      {  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f },
      { -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f },
      { -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f },
      { -0.5f, 0.5f,  0.5f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f },
      {  0.5f, 0.5f,  0.5f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f },

      // -Y
      {  0.5f, -0.5f,  0.5f, 1.0f, 0.0f,  0.0f, -1.0f, 0.0f },
      { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f },
      {  0.5f, -0.5f, -0.5f, 1.0f, 1.0f,  0.0f, -1.0f, 0.0f },
      { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f },
      {  0.5f, -0.5f,  0.5f, 1.0f, 0.0f,  0.0f, -1.0f, 0.0f },
      { -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f },
    };

    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(verts)),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&g_vb_unitcube)
    ));

    // Upload
    UINT8* pData;
    CD3DX12_RANGE readRange(0, 0);
    CE(g_vb_unitcube->Map(0, &readRange, (void**)(&pData)));
    memcpy(pData, verts, sizeof(verts));
    g_vb_unitcube->Unmap(0, nullptr);

    g_vbv_unitcube.BufferLocation = g_vb_unitcube->GetGPUVirtualAddress();
    g_vbv_unitcube.StrideInBytes = sizeof(VertexUVNormal);
    g_vbv_unitcube.SizeInBytes = sizeof(verts);
  }

  // Create vertex buffer & view for the full-screen quad
  {
    VertexUV verts[] = {
      {  1.0f,  1.0f, 0.0f,  1.0f, 0.0f }, //  
      {  1.0f, -1.0f, 0.0f,  1.0f, 1.0f }, //  +-----------+ UV = (1, 0), NDC=(1, 1)
      { -1.0f, -1.0f, 0.0f,  0.0f, 1.0f }, //  |           |
                                           //  |           |
      { -1.0f, -1.0f, 0.0f,  0.0f, 1.0f }, //  |           |
      { -1.0f,  1.0f, 0.0f,  0.0f, 0.0f }, //  |           |
      {  1.0f,  1.0f, 0.0f,  1.0f, 0.0f }, //  +-----------+ UV = (1, 1), NDC=(1,-1)

      {  1.0f,  1.0f, 0.0f,  1.0f, 0.0f }, //  
      {  1.0f, -1.0f, 0.0f,  1.0f, 1.0f }, //  +-----------+ UV = (1, 0), NDC=(1, 1)
      { -1.0f, -1.0f, 0.0f,  0.0f, 1.0f }, //  |           |
                                           //  |           |
      { -1.0f, -1.0f, 0.0f,  0.0f, 1.0f }, //  |           |
      { -1.0f,  1.0f, 0.0f,  0.0f, 0.0f }, //  |           |
      {  1.0f,  1.0f, 0.0f,  1.0f, 0.0f }, //  +-----------+ UV = (1, 1), NDC=(1,-1)
    };
    CE(g_device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(verts)),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr, IID_PPV_ARGS(&g_vb_fsquad)
    ));

    // Upload
    UINT8* pData;
    CD3DX12_RANGE readRange(0, 0);
    CE(g_vb_fsquad->Map(0, &readRange, (void**)(&pData)));
    memcpy(pData, verts, sizeof(verts));
    g_vb_fsquad->Unmap(0, nullptr);

    g_vbv_fsquad.BufferLocation = g_vb_fsquad->GetGPUVirtualAddress();
    g_vbv_fsquad.StrideInBytes = sizeof(VertexUV);
    g_vbv_fsquad.SizeInBytes = sizeof(verts);
  }

  {
    const float EPS = 0.007f;
    VertexAndColor verts[] = { // Treat as trianglez
      { 0.5f,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f },     //     -+ 右
      { 0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },    //     \|
      { 0.5f - EPS,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f }, //      |

      { 0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },    //
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },   //       __-  下
      { 0.5f, -0.5f + EPS, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },//   ------+

      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },    //   |
      { -0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },    //   |\   左
      { -0.5f + EPS, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },//   +-

      { -0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },    //
      { 0.5f,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f },      //   +------   上
      { -0.5f,  0.5f - EPS, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },//   ---
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


void InitTextureAndSRVs() {

  CD3DX12_CPU_DESCRIPTOR_HANDLE h(g_srvheap->GetCPUDescriptorHandleForHeapStart());

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;

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
    g_device->CreateShaderResourceView(texture, &srvDesc, h);
    h.Offset(g_cbvDescriptorSize);
  }

  // Main canvas and light mask
  {
    g_device->CreateShaderResourceView(g_maincanvas, &srvDesc, h);
    h.Offset(g_cbvDescriptorSize);

    g_device->CreateShaderResourceView(g_scatterlight, &srvDesc, h);
    h.Offset(g_cbvDescriptorSize);
  }
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


void InitConstantBuffer() {
  // CBV
  CE(g_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(2048 * 64),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&g_constantbuffer)));

  CE(g_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(256),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&g_constantbuffer_drawlight)
  ));

  const unsigned SIZE = 256 * (NUM_OBJECTS - NUM_TEXTURES); // Assume we have 100 objects
  // Describe and create a constant buffer view.
  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
  cbvDesc.BufferLocation = g_constantbuffer->GetGPUVirtualAddress();
  cbvDesc.SizeInBytes = SIZE;

  CD3DX12_CPU_DESCRIPTOR_HANDLE handle1(g_srvheap->GetCPUDescriptorHandleForHeapStart(), NUM_TEXTURES + NUM_SRVS, g_cbvDescriptorSize);
  g_device->CreateConstantBufferView(&cbvDesc, handle1);

  // Map and initialize the constant buffer. We don't unmap this until the
  // app closes. Keeping things mapped for the lifetime of the resource is okay.
  CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
  CE(g_constantbuffer->Map(0, &readRange, (void**)&g_pCbvDataBegin));
  memcpy(g_pCbvDataBegin, &g_constantbufferdata, sizeof(g_constantbufferdata));
  g_constantbuffer->Unmap(0, NULL);

  cbvDesc.BufferLocation = g_constantbuffer_drawlight->GetGPUVirtualAddress();
  cbvDesc.SizeInBytes = 256;
  handle1 = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_srvheap->GetCPUDescriptorHandleForHeapStart(), NUM_TEXTURES + NUM_SRVS + 1,
    g_cbvDescriptorSize);
  g_device->CreateConstantBufferView(&cbvDesc, handle1);
}

void EndInitializeFence() {
  CE(g_commandlist->Close());
  CE(g_commandlist_bb->Close());
  CE(g_commandlist_copier->Close());
  ID3D12CommandList* ppCommandLists[] = { g_commandlist };
  g_commandqueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
  CE(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
  g_fencevalue = 1;
  g_fenceevent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  WaitForPreviousFrame();
}

void Update_DX12() {
  const size_t N = g_spriteInstances.size();
  const UINT ALIGN = 256;
  CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
  {
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
      d.win_h = WIN_H * 1.0f;
      d.win_w = WIN_W * 1.0f;
      d.orientation = sInst->orientation;
      memcpy(g_pCbvDataBegin + (1 + i)*ALIGN, &d, sizeof(d));
    }
    g_constantbuffer->Unmap(0, nullptr);
  }

  {
    g_constantbufferdata_drawlight.light_x = WIN_W * 0.25f;
    g_constantbufferdata_drawlight.light_y = WIN_H * 0.25f;
    g_constantbufferdata_drawlight.light_r = 150.0f;
    g_constantbufferdata_drawlight.WIN_H = WIN_H * 1.0f;
    g_constantbufferdata_drawlight.WIN_W = WIN_W * 1.0f;
    g_constantbufferdata_drawlight.light_color.m128_f32[0] = 1.0f;
    g_constantbufferdata_drawlight.light_color.m128_f32[1] = 1.0f;
    g_constantbufferdata_drawlight.light_color.m128_f32[2] = 1.0f;
    g_constantbufferdata_drawlight.light_color.m128_f32[3] = 1.0f;
    g_constantbufferdata_drawlight.global_alpha = 1.0f;

    CE(g_constantbuffer_drawlight->Map(0, &readRange, (void**)&g_pCbvDataBegin));
    memcpy(g_pCbvDataBegin, &g_constantbufferdata_drawlight, sizeof(g_constantbufferdata_drawlight));
    g_constantbuffer_drawlight->Unmap(0, nullptr);
  }
}

void Render_DX12() {
  // Populate Command List
  CE(g_commandallocator->Reset());
  CE(g_commandlist->Reset(g_commandallocator, g_pipelinestate));
  CE(g_commandallocator_bb->Reset());
  CE(g_commandlist_bb->Reset(g_commandallocator_bb, g_pipelinestate_bb));
  CE(g_commandallocator_copier->Reset());
  CE(g_commandlist_copier->Reset(g_commandallocator_copier, g_pipelinestate_copier));

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle_canvas, rtvHandle, rtvHandle_lightmask, dsvHandle;
  CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle;

  g_commandlist->SetGraphicsRootSignature(g_rootsignature);
  g_commandlist_bb->SetGraphicsRootSignature(g_rootsignature);
  g_commandlist_copier->SetGraphicsRootSignature(g_rootsignature);

  // Bind resources

  ID3D12DescriptorHeap* ppHeaps[] = { g_srvheap };
  g_commandlist->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  g_commandlist_bb->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
  g_commandlist_copier->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  g_commandlist->RSSetViewports(1, &g_viewport);
  g_commandlist->RSSetScissorRects(1, &g_scissorrect);
  g_commandlist_bb->RSSetViewports(1, &g_viewport);
  g_commandlist_bb->RSSetScissorRects(1, &g_scissorrect);
  g_commandlist_copier->RSSetViewports(1, &g_viewport);
  g_commandlist_copier->RSSetScissorRects(1, &g_scissorrect);

  // Blending
  float blend_factor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  g_commandlist->OMSetBlendFactor(blend_factor);

  // Draw stuff

  g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_maincanvas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET));

  // 这个barrier需要放到这里以保证正确性
  g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_scatterlight, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET));

  rtvHandle_canvas = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_descriptorheap->GetCPUDescriptorHandleForHeapStart(), FrameCount, g_rtvDescriptorSize);
  rtvHandle_lightmask = rtvHandle_canvas;
  rtvHandle_lightmask.Offset(g_rtvDescriptorSize);
  rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_descriptorheap->GetCPUDescriptorHandleForHeapStart(), g_frameindex, g_rtvDescriptorSize);

  dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_dsv_heap->GetCPUDescriptorHandleForHeapStart());

  const float* clearColor = g_scene_background;

  g_commandlist->OMSetRenderTargets(1, &rtvHandle_canvas, FALSE, &dsvHandle);
  g_commandlist_bb->OMSetRenderTargets(1, &rtvHandle_canvas, FALSE, nullptr);

  g_commandlist->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  g_commandlist->ClearRenderTargetView(rtvHandle_canvas, clearColor, 0, nullptr);
  g_commandlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  g_commandlist_bb->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_commandlist_bb->IASetVertexBuffers(0, 1, &g_vertexbufferview1);

  g_commandlist->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress()); // the CB is in slot 1 of the root signature...

  if (g_showBoundingBox) {
    // Make debug layer happy
    gpuSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_srvheap->GetGPUDescriptorHandleForHeapStart());
    g_commandlist_bb->SetGraphicsRootDescriptorTable(0, gpuSrvHandle);
    g_commandlist_bb->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress());
    g_commandlist_bb->SetGraphicsRootConstantBufferView(2, g_constantbuffer->GetGPUVirtualAddress());

    g_commandlist_bb->DrawInstanced(12, 1, 0, 0);
  }

  const size_t N = g_spriteInstances.size();
  const UINT ALIGN = 256;

  g_commandlist->SetGraphicsRootConstantBufferView(2, g_constantbuffer->GetGPUVirtualAddress());

  for (int i = 0; i < N; i++) {
    SpriteInstance* pSprInst = g_spriteInstances[i];
    if (pSprInst->Visible() == false) continue;
    const int textureId = pSprInst->pSprite->textureId;

    gpuSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_srvheap->GetGPUDescriptorHandleForHeapStart());
    gpuSrvHandle.Offset(textureId * g_cbvDescriptorSize);

    int num_verts = 6;

    if (IsWallTexture(textureId)) {
      g_commandlist->IASetVertexBuffers(0, 1, &g_vbv_unitcube);
      num_verts = 30;
    }
    else {
      g_commandlist->IASetVertexBuffers(0, 1, &g_vbv_unitsquare);
      num_verts = 6;
    }

    g_commandlist->SetGraphicsRootDescriptorTable(0, gpuSrvHandle);
    g_commandlist->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress() + ALIGN * (1 + i)); // the CB is in slot 1 of the root signature...
    g_commandlist->DrawInstanced(num_verts, 1, 0, 0);

    if (g_showBoundingBox) {
      g_commandlist_bb->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress() + ALIGN * (1 + i)); // the CB is in slot 1 of the root signature...
      g_commandlist_bb->DrawInstanced(num_verts, 1, 0, 0);
    }
  }

  CE(g_commandlist->Close());
  CE(g_commandlist_bb->Close());

  g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist));
  g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist_bb));

  {
    // Render light
    g_commandlist->Reset(g_commandallocator, g_pipelinestate_drawlight);
    g_commandlist->SetGraphicsRootSignature(g_rootsignature_drawlight);
    g_commandlist->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    g_commandlist->RSSetViewports(1, &g_viewport);
    g_commandlist->RSSetScissorRects(1, &g_scissorrect);
    float zeros[] = { .0f, .0f, .0f, .0f };
    g_commandlist->OMSetRenderTargets(1, &rtvHandle_lightmask, FALSE, nullptr);
    g_commandlist->OMSetBlendFactor(blend_factor);
    g_commandlist->ClearRenderTargetView(rtvHandle_lightmask, zeros, 0, nullptr);
    g_commandlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandlist->IASetVertexBuffers(0, 1, &g_vbv_fsquad);

    g_commandlist->SetGraphicsRootConstantBufferView(0, g_constantbuffer_drawlight->GetGPUVirtualAddress());
    g_commandlist->DrawInstanced(6, 1, 0, 0);

    CE(g_commandlist->Close());
    g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist));

  }

  {
    // Render light masks
    g_commandlist->Reset(g_commandallocator, g_pipelinestate_lightmask);
    g_commandlist->SetGraphicsRootSignature(g_rootsignature);
    g_commandlist->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    g_commandlist->RSSetViewports(1, &g_viewport);
    g_commandlist->RSSetScissorRects(1, &g_scissorrect);
    g_commandlist->OMSetRenderTargets(1, &rtvHandle_lightmask, FALSE, nullptr);
    g_commandlist->OMSetBlendFactor(blend_factor);
    g_commandlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandlist->IASetVertexBuffers(0, 1, &g_vbv_unitsquare);

    g_commandlist->SetGraphicsRootConstantBufferView(2, g_constantbuffer->GetGPUVirtualAddress());

    for (int i = 0; i < N; i++) {
      SpriteInstance* pSprInst = g_spriteInstances[i];
      if (pSprInst->Visible() == false) continue;
      const int textureId = pSprInst->pSprite->textureId;

      gpuSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_srvheap->GetGPUDescriptorHandleForHeapStart());
      gpuSrvHandle.Offset(textureId * g_cbvDescriptorSize);

      g_commandlist->SetGraphicsRootDescriptorTable(0, gpuSrvHandle);
      g_commandlist->SetGraphicsRootConstantBufferView(1, g_constantbuffer->GetGPUVirtualAddress() + ALIGN * (1 + i)); // the CB is in slot 1 of the root signature...
      g_commandlist->DrawInstanced(6, 1, 0, 0);
    }

    g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_scatterlight, D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    CE(g_commandlist->Close());
    g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist));
  }

  {
    // Combine
    g_commandlist->Reset(g_commandallocator, g_pipelinestate_combine);

    g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_maincanvas, D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    g_commandlist->SetGraphicsRootSignature(g_rootsignature_combine);
    g_commandlist->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    g_commandlist->RSSetViewports(1, &g_viewport);
    g_commandlist->RSSetScissorRects(1, &g_scissorrect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(g_descriptorheap->GetCPUDescriptorHandleForHeapStart(), g_frameindex * g_rtvDescriptorSize);
    g_commandlist->OMSetRenderTargets(1, &cpu_handle, true, nullptr);

    g_commandlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandlist->IASetVertexBuffers(0, 1, &g_vbv_fsquad);

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle1(g_srvheap->GetGPUDescriptorHandleForHeapStart(), NUM_TEXTURES * g_cbvDescriptorSize);
    g_commandlist->SetGraphicsRootDescriptorTable(0, gpu_handle1);

    g_commandlist->SetGraphicsRootConstantBufferView(1, g_constantbuffer_drawlight->GetGPUVirtualAddress());

    g_commandlist->DrawInstanced(6, 1, 0, 0);

    g_commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    CE(g_commandlist->Close());
    g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist));
  }

  if (0) {
    g_commandlist_copier->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_maincanvas, D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_COPY_SOURCE));
    g_commandlist_copier->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

    g_commandlist_copier->CopyResource(g_rendertargets[g_frameindex], g_maincanvas);

    g_commandlist_copier->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_rendertargets[g_frameindex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

  }

  CE(g_commandlist_copier->Close());
  // Execute Command List
  g_commandqueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)(&g_commandlist_copier));

  // Flip the swapchain
  CE(g_swapchain->Present(1, 0));

  WaitForPreviousFrame();
}
