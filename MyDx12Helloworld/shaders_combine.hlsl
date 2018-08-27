struct PSInput
{
  float4 position : SV_POSITION;
  float2 uv : TEXCOORD;
};

Texture2D g_src1 : register(t0);
Texture2D g_src2 : register(t1);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 uv : TEXCOORD)
{
  PSInput result;
  result.position = position;
  result.uv = uv.xy;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
  float4 rgba1 = g_src1.Sample(g_sampler, input.uv);
  float2 delta = input.uv - float2(0.5f, 0.5f);
  float4 rgba2 = g_src2.Sample(g_sampler, input.uv - delta * 0.05f);
  if (rgba1.a <= 0) {
    discard;
    return rgba1; // Still need to return
  } else {
    return rgba1 + (rgba2*0.3f);
  }
}
