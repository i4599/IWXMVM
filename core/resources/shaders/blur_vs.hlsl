struct VS_INPUT
{
    float3 pos : POSITION;
};

struct VS_OUTPUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

uniform float2 texel_offset : register(c0);

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT ret;

    ret.pos = float4(input.pos, 1.0f);
    ret.pos.xy += texel_offset;

    ret.uv = float2(0.5f, -0.5f) * input.pos.xy + 0.5f;

    return ret;
}
