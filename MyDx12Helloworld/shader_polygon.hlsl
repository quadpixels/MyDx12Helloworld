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

struct GSInput
{
  float4 position : SV_POSITION;
  float4 color : COLOR;
};

struct PSInput
{
  float4 position : SV_POSITION;
  float4 color : COLOR;
};

cbuffer PerSceneCB : register(b1)
{
  float4x4 view;
  float4x3 projection;
  float win_w, win_h;
}

// Add this: NULL error
cbuffer SceneConstantBuffer : register(b0)
{
  float x, y, z;
  float w, h;
  float4x4 orientation;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
  PSInput result;

  float min_wh = min(win_h, win_w);
  float half_h = h / 100;
  float half_w = w / 100;
  result.position = mul(orientation, position)  // Unit square
    * float4(half_w, half_h, 1.0f, 1.0f) // Aspect ratio correction
    + float4(x / 100, y / 100, 0, 0); // Translation
  result.color = color;

  result.position = mul(projection, mul(view, result.position));
  result.position.z /= 100.0f;

  return result;
}


// P[1] +      +   <--- P[3]: mirror
//      | \_
//      |   \_
// P[0] +------+ P[2]
[maxvertexcount(6)]
void GSMain(triangle GSInput input[3],
  inout TriangleStream<PSInput> triStream) {

  PSInput ret;

  
  ret = input[0];
  ret.color = float4(0.0f, 1.0f, 1.0f, 1.0f);
  triStream.Append(ret);

  ret = input[1];
  ret.color = float4(0.0f, 1.0f, 1.0f, 1.0f);
  triStream.Append(ret);

  ret = input[2];
  ret.color = float4(0.0f, 1.0f, 1.0f, 1.0f);
  triStream.Append(ret);

  triStream.RestartStrip(); // 这个strip已经完成，前进至下一个strip

  float4 mid = (input[1].position + input[2].position) * 0.5f;
  float4 mirrored = mid * 2.0f - input[0].position;

  ret.position = input[2].position;
  ret.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
  triStream.Append(ret);

  ret.position = input[1].position;
  triStream.Append(ret);

  ret.position = mirrored;
  triStream.Append(ret);
}

float4 PSMain(PSInput input) : SV_TARGET
{
  return input.color;
}

