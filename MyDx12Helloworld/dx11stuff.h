// D3D11
ID3D11Device1* g_device11;
ID3D11DeviceContext1* g_context11;
IDXGISwapChain* g_swapchain11;
ID3D11Buffer* g_vb_unitsquare11;
ID3D11Buffer* g_vb_unitcube11;
ID3D11Buffer* g_vb_fsquad11;
ID3D11Buffer* g_vb_boundingbox11;
std::vector<ID3D11ShaderResourceView*> g_srvs11;
ID3D11Texture2D* g_maincanvas11;
ID3D11Texture2D* g_lightmask11;
ID3D11Texture2D* g_depthbuffer11;
ID3D11Texture2D* g_scatterlight11;
ID3D11RenderTargetView* g_maincanvas11_rtv;
ID3D11ShaderResourceView* g_maincanvas11_srv;
ID3D11RenderTargetView* g_lightmask11_rtv;
ID3D11ShaderResourceView* g_lightmask11_srv;
ID3D11DepthStencilView* g_dsv11;
ID3D11Buffer* g_constantbuffer_perscene11;
ID3D11Buffer* g_constantbuffer_perobject11;
ID3D11Buffer* g_constantbuffer_drawlight11;
ID3D11Texture2D *g_backbuffer;
ID3D11RenderTargetView* g_backbuffer_rtv11;
D3D11_VIEWPORT g_viewport11;
D3D11_RECT g_scissorrect11;
ID3D11InputLayout* g_inputlayout_withnormal11;
ID3D11InputLayout* g_inputlayout_nonormal11;
ID3D11VertexShader *g_vs_objects11;
ID3D11PixelShader* g_ps_objects11;
ID3D11VertexShader *g_vs_drawlight11;
ID3D11PixelShader* g_ps_drawlight11;
ID3D11VertexShader* g_vs_lightmask11;
ID3D11PixelShader* g_ps_lightmask11;
ID3D11VertexShader* g_vs_combine11;
ID3D11PixelShader* g_ps_combine11;
ID3D11InputLayout* g_inputlayout_boundingbox11;
ID3D11VertexShader* g_vs_boundingbox11;
ID3D11GeometryShader* g_gs_boundingbox11;
ID3D11PixelShader* g_ps_boundingbox11;
ID3D11SamplerState* g_sampler11;
ID3D11DepthStencilState* g_depthstencil11, *g_depthstencil_lightmask11, *g_depthstencil_bb11;
ID3D11RasterizerState* g_rasterizerstate11;
ID3D11BlendState* g_blendstate11;

void InitDevice11() {
  // 1. Create Device and Swap Chain
  D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_1 };

  DXGI_SWAP_CHAIN_DESC scd = { };
  scd.BufferDesc.Width = WIN_W;
  scd.BufferDesc.Height = WIN_H;
  scd.BufferDesc.RefreshRate.Numerator = 0;
  scd.BufferDesc.RefreshRate.Denominator = 0;
  scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  scd.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
  scd.SampleDesc.Count = 1;
  scd.SampleDesc.Quality = 0;
  scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  scd.BufferCount = 2;
  scd.OutputWindow = g_hwnd;
  scd.Windowed = TRUE;
  scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  scd.Flags = 0;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    0,
    feature_levels,
    _countof(feature_levels),
    D3D11_SDK_VERSION,
    &scd,
    &g_swapchain11,
    (ID3D11Device**)&g_device11,
    nullptr,
    nullptr);
  assert(hr == S_OK);
  g_device11->GetImmediateContext1(&g_context11);

  hr = g_swapchain11->GetBuffer(0, IID_PPV_ARGS(&g_backbuffer));
  assert(SUCCEEDED(hr));

  hr = g_device11->CreateRenderTargetView(g_backbuffer, nullptr, &g_backbuffer_rtv11);
  assert(SUCCEEDED(hr));

  g_viewport11.Height = WIN_H;
  g_viewport11.Width = WIN_W;
  g_viewport11.TopLeftX = 0;
  g_viewport11.TopLeftY = 0;
  g_viewport11.MinDepth = 0;
  g_viewport11.MaxDepth = 1; // Need to set otherwise depth will be all -1's

  g_scissorrect11.left = 0;
  g_scissorrect11.top = 0;
  g_scissorrect11.bottom = WIN_H;
  g_scissorrect11.right = WIN_W;
}

void InitAssets11() {
  UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

  ID3DBlob* error = nullptr;
  CE(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  HRESULT result = g_device11->CreateVertexShader(g_VS->GetBufferPointer(), g_VS->GetBufferSize(), nullptr, &g_vs_objects11);
  assert(SUCCEEDED(result));

  CE(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreatePixelShader(g_PS->GetBufferPointer(), g_PS->GetBufferSize(), nullptr, &g_ps_objects11);

  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS_boundingbox, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreateVertexShader(g_VS_boundingbox->GetBufferPointer(), g_VS_boundingbox->GetBufferSize(), nullptr, &g_vs_boundingbox11);
  assert(SUCCEEDED(result));
  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS_boundingbox, &error));
  result = g_device11->CreatePixelShader(g_PS_boundingbox->GetBufferPointer(), g_PS_boundingbox->GetBufferSize(), nullptr, &g_ps_boundingbox11);
  assert(SUCCEEDED(result));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  CE(D3DCompileFromFile(L"shader_polygon.hlsl", nullptr, nullptr, "GSMain", "gs_5_0", compileFlags, 0, &g_GS_boundingbox, &error));
  if (error) printf("Error compiling GS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreateGeometryShader(g_GS_boundingbox->GetBufferPointer(), g_GS_boundingbox->GetBufferSize(), nullptr, &g_gs_boundingbox11);
  assert(SUCCEEDED(result));

  CE(D3DCompileFromFile(L"shaders_mask.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS2, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreateVertexShader(g_VS2->GetBufferPointer(), g_VS2->GetBufferSize(), nullptr, &g_vs_lightmask11);
  CE(D3DCompileFromFile(L"shaders_mask.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS2, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreatePixelShader(g_PS2->GetBufferPointer(), g_PS2->GetBufferSize(), nullptr, &g_ps_lightmask11);


  CE(D3DCompileFromFile(L"shaders_combine.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS_combine, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreateVertexShader(g_VS_combine->GetBufferPointer(), g_VS_combine->GetBufferSize(), nullptr, &g_vs_combine11);
  CE(D3DCompileFromFile(L"shaders_combine.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS_combine, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreatePixelShader(g_PS_combine->GetBufferPointer(), g_PS_combine->GetBufferSize(), nullptr, &g_ps_combine11);

  CE(D3DCompileFromFile(L"shaders_drawlight.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_VS_drawlight, &error));
  if (error) printf("Error compiling VS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreateVertexShader(g_VS_drawlight->GetBufferPointer(), g_VS_drawlight->GetBufferSize(), nullptr, &g_vs_drawlight11);
  assert(SUCCEEDED(result));
  CE(D3DCompileFromFile(L"shaders_drawlight.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_PS_drawlight, &error));
  if (error) printf("Error compiling PS: %s\n", (char*)error->GetBufferPointer());
  result = g_device11->CreatePixelShader(g_PS_drawlight->GetBufferPointer(), g_PS_drawlight->GetBufferSize(), nullptr, &g_ps_drawlight11);
  assert(SUCCEEDED(result));

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

    D3D11_BUFFER_DESC buf_desc = { };
    buf_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buf_desc.ByteWidth = sizeof(verts);
    buf_desc.StructureByteStride = sizeof(VertexUVNormal);
    buf_desc.Usage = D3D11_USAGE_IMMUTABLE;

    D3D11_SUBRESOURCE_DATA srd = { };
    srd.pSysMem = verts;
    
    HRESULT hr = g_device11->CreateBuffer(&buf_desc, &srd, &g_vb_unitsquare11);
    assert(SUCCEEDED(hr));
  }

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

    D3D11_BUFFER_DESC desc = { };
    desc.ByteWidth = sizeof(verts);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.StructureByteStride = sizeof(VertexUVNormal);
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA srd = { };
    srd.pSysMem = verts;
    
    HRESULT hr = g_device11->CreateBuffer(&desc, &srd, &g_vb_unitcube11);
    assert(SUCCEEDED(hr));
  }

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

    D3D11_BUFFER_DESC desc = { };
    desc.ByteWidth = sizeof(verts);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.StructureByteStride = sizeof(VertexUVNormal);
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA srd = { };
    srd.pSysMem = verts;

    HRESULT hr = g_device11->CreateBuffer(&desc, &srd, &g_vb_fsquad11);
    assert(SUCCEEDED(hr));
  }

  {
    const float EPS = 0.05f;
    VertexAndColor verts[] = { // Treat as trianglez
      { 0.5f,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f },     //    --+ 右
      { 0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },    //     \|
      { 0.5f - EPS,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f }, //    |

      { 0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },      //
      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },     //     __-  下
      { 0.5f, -0.5f + EPS, 0.0f,  1.0f, 1.0f, 0.0f, 1.0f },// ------+

      { -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },      //   |
      { -0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },      //   |\   左
      { -0.5f + EPS, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f },//   +--

      { -0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },      //
      { 0.5f,  0.5f, 0.0f,  1.0f, 0.0, 0.0f, 1.0f },        //   +------   上
      { -0.5f,  0.5f - EPS, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f },//   ---
    };
    D3D11_BUFFER_DESC desc = { };
    desc.ByteWidth = sizeof(verts);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.StructureByteStride = sizeof(VertexAndColor);
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = { };
    srd.pSysMem = verts;
    assert(SUCCEEDED(g_device11->CreateBuffer(&desc, &srd, &g_vb_boundingbox11)));
  }

  // Input Layouts
  {
    D3D11_INPUT_ELEMENT_DESC inputdesc1[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    result = g_device11->CreateInputLayout(inputdesc1, 3, g_VS->GetBufferPointer(), g_VS->GetBufferSize(), &g_inputlayout_withnormal11);
    assert(SUCCEEDED(result));

    D3D11_INPUT_ELEMENT_DESC inputdesc2[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    assert(SUCCEEDED(g_device11->CreateInputLayout(inputdesc1, 2, g_VS_combine->GetBufferPointer(), g_VS_combine->GetBufferSize(), &g_inputlayout_nonormal11)));

    D3D11_INPUT_ELEMENT_DESC inputdesc3[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    assert(SUCCEEDED(g_device11->CreateInputLayout(inputdesc3, 2, g_VS_boundingbox->GetBufferPointer(), g_VS_boundingbox->GetBufferSize(), &g_inputlayout_boundingbox11)));
  }

  // Sampler State
  {
    D3D11_SAMPLER_DESC sd = { };
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.MaxAnisotropy = 1;
    sd.MinLOD = -FLT_MAX;
    sd.MaxLOD = FLT_MAX;
    HRESULT result = g_device11->CreateSamplerState(&sd, &g_sampler11);
    assert(SUCCEEDED(result));
  }

  // Depth-Stencil States
  {
    // For the color buffer ...
    D3D11_DEPTH_STENCIL_DESC dsd = { };
    dsd.DepthEnable = true;
    dsd.StencilEnable = false;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    assert(SUCCEEDED(g_device11->CreateDepthStencilState(&dsd, &g_depthstencil11)));

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    assert(SUCCEEDED(g_device11->CreateDepthStencilState(&dsd, &g_depthstencil_bb11)));

    // For the light mask.
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    assert(SUCCEEDED(g_device11->CreateDepthStencilState(&dsd, &g_depthstencil_lightmask11)));
  }

  // Rasterizer State
  {
    D3D11_RASTERIZER_DESC rsd = { };
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.DepthBias = 0;
    rsd.DepthClipEnable = true;
    HRESULT result = g_device11->CreateRasterizerState(&rsd, &g_rasterizerstate11);
    assert(SUCCEEDED(result));
  }

  // Blend state
  {
    D3D11_BLEND_DESC bd = { };
    bd.AlphaToCoverageEnable = false;
    bd.IndependentBlendEnable = false;
    bd.RenderTarget[0].BlendEnable = true;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT result = g_device11->CreateBlendState(&bd, &g_blendstate11);
  }
}

void InitTextureAndSRVs11() {
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
  srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture1D.MipLevels = 1;
  
  // Read-only textures
  for (int i = 0; i < NUM_TEXTURES; i++) {
    ID3D11Texture2D* texture;

    UINT tex_w, tex_h;
    unsigned char* rgba = LoadTexture(LPCWSTR(texNames[i]), &tex_w, &tex_h);

    D3D11_TEXTURE2D_DESC t2d = { };
    t2d.MipLevels = 1; // Must be 1: https://gamedev.stackexchange.com/questions/148222/directx11-create-texture2d-using-subresourcedata
    t2d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    t2d.Width = tex_w;
    t2d.Height = tex_h;
    t2d.ArraySize = 1;
    t2d.SampleDesc.Count = 1;
    t2d.SampleDesc.Quality = 0;
    t2d.BindFlags = D3D11_BIND_SHADER_RESOURCE; // Must specify per DX debug info
    
    D3D11_SUBRESOURCE_DATA srd = { };
    srd.pSysMem = rgba;
    srd.SysMemPitch = 4 * tex_w;
    srd.SysMemSlicePitch = 4 * tex_w * tex_h;

    HRESULT result;
    result = g_device11->CreateTexture2D(&t2d, &srd, &texture);
    assert(SUCCEEDED(result));
    
    ID3D11ShaderResourceView* srv;
    result = g_device11->CreateShaderResourceView(texture, &srv_desc, &srv);
    assert(SUCCEEDED(result));
    g_srvs11.push_back(srv);
  }

  // Main canvas and light mask
  {
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D11_TEXTURE2D_DESC t2d = { };
    t2d.MipLevels = 1;
    t2d.Format = format;
    t2d.Width = WIN_W;
    t2d.Height = WIN_H;
    t2d.ArraySize = 1;
    t2d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    t2d.SampleDesc.Count = 1;
    t2d.SampleDesc.Quality = 0;
    t2d.Usage = D3D11_USAGE_DEFAULT;

    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = { };
    rtv_desc.Format = format;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    // Main canvas resource & main canvas RTV & Main canvas SRV
    HRESULT result;
    result = g_device11->CreateTexture2D(&t2d, nullptr, &g_maincanvas11);
    assert(SUCCEEDED(result));
    
    result = g_device11->CreateRenderTargetView(g_maincanvas11, &rtv_desc, &g_maincanvas11_rtv);
    assert(SUCCEEDED(result));

    result = g_device11->CreateShaderResourceView(g_maincanvas11, &srv_desc, &g_maincanvas11_srv);
    assert(SUCCEEDED(result));

    // Light mask resource & light mask RTV & light mask SRV
    result = g_device11->CreateTexture2D(&t2d, nullptr, &g_lightmask11);
    assert(SUCCEEDED(result));

    result = g_device11->CreateRenderTargetView(g_lightmask11, &rtv_desc, &g_lightmask11_rtv);
    assert(SUCCEEDED(result));

    result = g_device11->CreateShaderResourceView(g_lightmask11, &srv_desc, &g_lightmask11_srv);
    assert(SUCCEEDED(result));
  }

  // Depth-Stencil buffer & View
  {
    D3D11_TEXTURE2D_DESC d2d = { };
    d2d.MipLevels = 1;
    d2d.Format = DXGI_FORMAT_D32_FLOAT;
    d2d.Width = WIN_W;
    d2d.Height = WIN_H;
    d2d.ArraySize = 1;
    d2d.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    d2d.SampleDesc.Count = 1;
    d2d.SampleDesc.Quality = 0;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = { };
    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    HRESULT result;
    result = g_device11->CreateTexture2D(&d2d, nullptr, &g_depthbuffer11);
    assert(SUCCEEDED(result));

    result = g_device11->CreateDepthStencilView(g_depthbuffer11, &dsv_desc, &g_dsv11);
    assert(SUCCEEDED(result));
  }
}

void InitConstantBuffer11() {
  HRESULT result;

  D3D11_BUFFER_DESC buf_desc = { };
  buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  buf_desc.StructureByteStride = sizeof(PerSceneCBData);
  buf_desc.ByteWidth = sizeof(PerSceneCBData);
  buf_desc.Usage = D3D11_USAGE_DYNAMIC;
  buf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

  result = g_device11->CreateBuffer(&buf_desc, nullptr, &g_constantbuffer_perscene11);
  assert(SUCCEEDED(result));

  buf_desc.StructureByteStride = 256;// sizeof(ConstantBufferData);
  buf_desc.ByteWidth = 256 * 2048;// sizeof(ConstantBufferData) * 2048;
  result = g_device11->CreateBuffer(&buf_desc, nullptr, &g_constantbuffer_perobject11);

  buf_desc.StructureByteStride = sizeof(ConstantBufferDataDrawLight);
  buf_desc.ByteWidth = sizeof(ConstantBufferDataDrawLight);
  result = g_device11->CreateBuffer(&buf_desc, nullptr, &g_constantbuffer_drawlight11);
  assert(SUCCEEDED(result));
}

void Update_DX11() {
  // Per-scene CB
  {
    D3D11_MAPPED_SUBRESOURCE mapped;
    CE(g_context11->Map(g_constantbuffer_perscene11, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
    memcpy(mapped.pData, &g_per_scene_cb_data, sizeof(g_per_scene_cb_data));
    g_context11->Unmap(g_constantbuffer_perscene11, 0);
  }

  // Per-object CB
  // Offset is in units of 16 bytes (float4)
  // Use same alignment as in DX12 (256 B)
  // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1
  {
    const size_t N = g_spriteInstances.size();
    const UINT ALIGN = 256;
    D3D11_MAPPED_SUBRESOURCE mapped;
    CE(g_context11->Map(g_constantbuffer_perobject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
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
      memcpy((char*)(mapped.pData) + i * ALIGN, &d, sizeof(d));
    }
    g_context11->Unmap(g_constantbuffer_perobject11, 0);
  }

  // Drawlight CB
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

    D3D11_MAPPED_SUBRESOURCE mapped;
    CE(g_context11->Map(g_constantbuffer_drawlight11, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
    memcpy(mapped.pData, &g_constantbufferdata_drawlight, sizeof(g_constantbufferdata_drawlight));
    g_context11->Unmap(g_constantbuffer_drawlight11, 0);
  }
}

void Render_DX11() {
  g_context11->ClearDepthStencilView(g_dsv11, D3D11_CLEAR_DEPTH, 1.0f, 0);
  g_context11->OMSetRenderTargets(1, &g_maincanvas11_rtv, g_dsv11);
  g_context11->ClearRenderTargetView(g_maincanvas11_rtv, g_scene_background);
  g_context11->PSSetSamplers(0, 1, &g_sampler11);
  g_context11->OMSetDepthStencilState(g_depthstencil11, 0);
  g_context11->RSSetState(g_rasterizerstate11);

  const int N = int(g_spriteInstances.size());
  g_context11->RSSetScissorRects(1, &g_scissorrect11);
  g_context11->RSSetViewports(1, &g_viewport11);
  g_context11->VSSetShader(g_vs_objects11, nullptr, 0);
  g_context11->PSSetShader(g_ps_objects11, nullptr, 0);
  g_context11->VSSetConstantBuffers(1, 1, &g_constantbuffer_perscene11);
  g_context11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_context11->IASetInputLayout(g_inputlayout_withnormal11);

  // Base Pass
  for (int i = 0; i < N; i++) {
    SpriteInstance* pSprInst = g_spriteInstances.at(i);
    if (pSprInst->Visible() == false) continue;
    const int textureId = pSprInst->pSprite->textureId;

    int num_verts = 0;

    {
      UINT zero = 0;
      const UINT stride = sizeof(VertexUVNormal); // Strides are the same for the 2 kinds of vertices
      if (IsWallTexture(textureId)) {
        g_context11->IASetVertexBuffers(0, 1, &g_vb_unitcube11, &stride, &zero);
        num_verts = 30;
      }
      else {
        g_context11->IASetVertexBuffers(0, 1, &g_vb_unitsquare11, &stride, &zero);
        num_verts = 6;
      }
    }

    g_context11->PSSetShaderResources(0, 1, &g_srvs11.at(textureId));
    const UINT one = 16; // one = 1x float4, 16 = 16 x float4's
    const UINT offset = i * 16; // Alignment = 256 B = 16 float4's
    g_context11->VSSetConstantBuffers1(0, 1, &g_constantbuffer_perobject11, &offset, &one);
    g_context11->PSSetConstantBuffers1(0, 1, &g_constantbuffer_perobject11, &offset, &one);
    g_context11->Draw(num_verts, 0);
  }

  // Bounding Boxes
  if (g_showBoundingBox) {
    g_context11->OMSetRenderTargets(1, &g_maincanvas11_rtv, g_dsv11);
    g_context11->OMSetDepthStencilState(g_depthstencil_bb11, 0);
    g_context11->VSSetShader(g_vs_boundingbox11, nullptr, 0);
    g_context11->GSSetShader(g_gs_boundingbox11, nullptr, 0);
    g_context11->PSSetShader(g_ps_boundingbox11, nullptr, 0);
    UINT zero = 0;
    UINT stride = sizeof(VertexAndColor);
    g_context11->IASetVertexBuffers(0, 1, &g_vb_boundingbox11, &stride, &zero);
    ID3D11ShaderResourceView* nullsrv = nullptr;
    g_context11->PSSetShaderResources(0, 1, &nullsrv);
    for (int i = 0; i < N; i++) {
      SpriteInstance* pSprInst = g_spriteInstances.at(i);
      if (pSprInst->Visible() == false) continue;
      
      const UINT one = 16; //
      const UINT offset = i * 16; // A
      g_context11->VSSetConstantBuffers1(0, 1, &g_constantbuffer_perobject11, &offset, &one);
      g_context11->PSSetConstantBuffers1(0, 1, &g_constantbuffer_perobject11, &offset, &one);
      g_context11->Draw(12, 0);
    }
    g_context11->VSSetShader(nullptr, nullptr, 0);
    g_context11->GSSetShader(nullptr, nullptr, 0);
    g_context11->PSSetShader(nullptr, nullptr, 0);
  }

  // Render Light
  {
    // 1. Light Source
    g_context11->OMSetRenderTargets(1, &g_lightmask11_rtv, nullptr);
    float zeros[] = { 0,0,0,0 };
    g_context11->ClearRenderTargetView(g_lightmask11_rtv, zeros);
    UINT zero = 0;
    UINT stride = sizeof(VertexUV);
    g_context11->IASetVertexBuffers(0, 1, &g_vb_fsquad11, &stride, &zero);
    g_context11->PSSetConstantBuffers(0, 1, &g_constantbuffer_drawlight11);
    g_context11->PSSetShaderResources(0, 0, nullptr);
    g_context11->VSSetShader(g_vs_drawlight11, nullptr, 0);
    g_context11->PSSetShader(g_ps_drawlight11, nullptr, 0);
    g_context11->Draw(6, 0);

    // 2. Light blocked by objects
    g_context11->OMSetRenderTargets(1, &g_lightmask11_rtv, g_dsv11);
    g_context11->OMSetDepthStencilState(g_depthstencil_lightmask11, 0);
    g_context11->VSSetShader(g_vs_lightmask11, nullptr, 0);
    g_context11->VSSetConstantBuffers(1, 1, &g_constantbuffer_perscene11);
    g_context11->PSSetShader(g_ps_lightmask11, nullptr, 0);
    for (int i = 0; i < N; i++) {
      SpriteInstance* pSprInst = g_spriteInstances.at(i);
      if (pSprInst->Visible() == false) continue;
      const int textureId = pSprInst->pSprite->textureId;

      int num_verts = 0;
      {
        UINT zero = 0;
        const UINT stride = sizeof(VertexUVNormal); // Strides are the same for the 2 kinds of vertices
        if (IsWallTexture(textureId)) {
          g_context11->IASetVertexBuffers(0, 1, &g_vb_unitcube11, &stride, &zero);
          num_verts = 30;
        }
        else {
          g_context11->IASetVertexBuffers(0, 1, &g_vb_unitsquare11, &stride, &zero);
          num_verts = 6;
        }
      }
      g_context11->PSSetShaderResources(0, 1, &g_srvs11.at(textureId));
      const UINT one = 16;
      const UINT offset = i * 16; // Alignment = 256 B = 16 float4's
      g_context11->VSSetConstantBuffers1(0, 1, &g_constantbuffer_perobject11, &offset, &one);
      g_context11->PSSetConstantBuffers1(0, 1, &g_constantbuffer_perobject11, &offset, &one);
      g_context11->Draw(num_verts, 0);
    }
  }

  // 3. Combine & Render the Light-Scatter
  {
    float zero4[] = { 0,0,0,0 };
    g_context11->ClearRenderTargetView(g_backbuffer_rtv11, zero4);
    g_context11->OMSetRenderTargets(1, &g_backbuffer_rtv11, nullptr);
    g_context11->VSSetShader(g_vs_combine11, nullptr, 0);
    g_context11->PSSetShader(g_ps_combine11, nullptr, 0);
    UINT stride = sizeof(VertexUV), zero = 0;
    g_context11->IASetVertexBuffers(0, 1, &g_vb_fsquad11, &stride, &zero);
    ID3D11ShaderResourceView* srvs[] = { g_maincanvas11_srv, g_lightmask11_srv };
    g_context11->PSSetShaderResources(0, 2, srvs);
    g_context11->PSSetConstantBuffers(0, 1, &g_constantbuffer_drawlight11);
    g_context11->Draw(6, 0);
  }

  // Unbind
  {
    ID3D11ShaderResourceView* nullsrvs[] = { nullptr, nullptr };
    g_context11->PSSetShaderResources(0, 2, nullsrvs);
  }
 
  g_swapchain11->Present(1, 0);
}