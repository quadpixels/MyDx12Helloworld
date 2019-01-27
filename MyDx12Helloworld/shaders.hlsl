//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
  float4 position : SV_POSITION;
  float2 uv : TEXCOORD;
  float3 normal : NORMAL;
};

cbuffer PerSceneCB : register(b1)
{
  float4x4 view;
  float4x4 projection;
}

// Add this: NULL error
cbuffer PerObjectCB : register(b0)
{
  float x, y;
  float w, h;
  float win_w, win_h;
  float4x4 orientation;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);


PSInput VSMain(float4 position : POSITION, float4 uv : TEXCOORD, float3 normal : NORMAL)
{
  PSInput result;
  float min_wh = min(win_h, win_w);
  float half_h = h / 100;
  float half_w = w / 100;
  result.position = mul(orientation, position)  // Unit square
    * float4(half_w, half_h, 1.0f, 1.0f) // Aspect ratio correction
    + float4(x / 100, y / 100, 0, 0); // Translation
  
  float3 normal_world = mul(orientation, normal);

  result.position = mul(mul(projection, view), result.position);
  result.position.z /= 100.0f; // To make the depth between 0 and 1
  result.uv = uv.xy;
  result.normal = normal_world;

  return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
  float4 rgba = g_texture.Sample(g_sampler, input.uv);
  if (rgba.a <= 0) {
    discard;
    return rgba; // Still need to return
  }
  else {
    float3 light_pos = float3(0.0f, 0.0f, -100.0f);
    float3 lp = input.position.xyz / input.position.w - light_pos;
    float lpdotn = dot(normalize(-lp), input.normal);
    if (input.normal.x == 0.0f && input.normal.y == 0.0f && input.normal.z == 0.0f) lpdotn = 1.0f;
    lpdotn = clamp(lpdotn, 0.4f, 1.0f);
    rgba.xyz = rgba.xyz * lpdotn;
    return rgba;
  }
  //  return float4(1.0f, 1.0f, input.u, input.v);
}
