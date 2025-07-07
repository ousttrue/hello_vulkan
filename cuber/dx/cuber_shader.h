static auto SHADER = R"(
#pragma pack_matrix(row_major)
cbuffer c0 : register(b0)
{
float4x4 VP;
};
struct vs_in {
    float4 pos: POSITION;
    float4 uv_barycentric: TEXCOORD;
    float4 row0: ROW0;
    float4 row1: ROW1;
    float4 row2: ROW2;
    float4 row3: ROW3;    
    float4 positiveXyzFlag: FACE0;
    float4 negativeXyzFlag: FACE1;
    uint instanceID : SV_InstanceID;
};
struct vs_out {
    float4 position_clip: SV_POSITION;
    float4 uv_barycentric: TEXCOORD;
    uint3 paletteFlagFlag: COLOR;
};

float4x4 transform(float4 r0, float4 r1, float4 r2, float4 r3)
{
  return float4x4(
    r0.x, r0.y, r0.z, r0.w,
    r1.x, r1.y, r1.z, r1.w,
    r2.x, r2.y, r2.z, r2.w,
    r3.x, r3.y, r3.z, r3.w
  );
}

vs_out vs_main(vs_in IN) {
  vs_out OUT = (vs_out)0; // zero the memory first
  OUT.position_clip = mul(float4(IN.pos.xyz, 1), mul(transform(IN.row0, IN.row1, IN.row2, IN.row3), VP));
  OUT.uv_barycentric = IN.uv_barycentric;
  if(IN.pos.w==0.0)
  {
    OUT.paletteFlagFlag = uint3(IN.positiveXyzFlag.x, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }
  else if(IN.pos.w==1.0)
  {
    OUT.paletteFlagFlag = uint3(IN.positiveXyzFlag.y, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }
  else if(IN.pos.w==2.0)
  {
    OUT.paletteFlagFlag = uint3(IN.positiveXyzFlag.z, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }
  else if(IN.pos.w==3.0)
  {
    OUT.paletteFlagFlag = uint3(IN.negativeXyzFlag.x, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }
  else if(IN.pos.w==4.0)
  {
    OUT.paletteFlagFlag = uint3(IN.negativeXyzFlag.y, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }
  else if(IN.pos.w==5.0)
  {
    OUT.paletteFlagFlag = uint3(IN.negativeXyzFlag.z, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }
  else{
    OUT.paletteFlagFlag = uint3(0, 
      IN.positiveXyzFlag.w,
      IN.negativeXyzFlag.w);
  }

  return OUT;
}

cbuffer c1 : register(b1)
{
  float4 colors[32];
  float4 textures[32];
};
Texture2D texture0;
SamplerState sampler0;
Texture2D texture1;
SamplerState sampler1;
Texture2D texture2;
SamplerState sampler2;

float grid (float2 vBC, float width) {
  float3 bary = float3(vBC.x, vBC.y, 1.0 - vBC.x - vBC.y);
  float3 d = fwidth(bary);
  float3 a3 = smoothstep(d * (width - 0.5), d * (width + 0.5), bary);
  return min(a3.x, a3.y);
}

float4 ps_main(vs_out IN) : SV_TARGET {
  float value = grid(IN.uv_barycentric.zw, 1.0);
  float4 border = float4(value, value, value, 1.0);
  float4 color = colors[IN.paletteFlagFlag.x];
  float4 texel;
  if(textures[IN.paletteFlagFlag.x].x==0.0)
  {
    texel = texture0.Sample(sampler0, IN.uv_barycentric.xy);
  }
  else if(textures[IN.paletteFlagFlag.x].x==1.0)
  {
    texel = texture1.Sample(sampler1, IN.uv_barycentric.xy);
  }
  else if(textures[IN.paletteFlagFlag.x].x==2.0)
  {
    texel = texture2.Sample(sampler2, IN.uv_barycentric.xy);
  }
  else{
    texel = float4(1, 1, 1, 1);
  }

  return texel * color * border;
}
)";
