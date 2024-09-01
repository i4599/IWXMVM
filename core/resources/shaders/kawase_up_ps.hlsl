struct PS_INPUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

sampler2D back_buffer : register(s0);
float2 half_pixel : register(c0);

float4 main(PS_INPUT input) : COLOR
{
    return (       tex2Dlod(back_buffer, float4(input.uv + float2(2.0f * half_pixel.x, 0.0f), 0.0f, 0.0f))
            +      tex2Dlod(back_buffer, float4(input.uv + float2(-2.0f * half_pixel.x, 0.0f), 0.0f, 0.0f))
            +      tex2Dlod(back_buffer, float4(input.uv + float2(0.0f, 2.0f * half_pixel.y), 0.0f, 0.0f))
            +      tex2Dlod(back_buffer, float4(input.uv + float2(0.0f, -2.0f * half_pixel.y), 0.0f, 0.0f))
            + 2.0f*tex2Dlod(back_buffer, float4(input.uv + float2(half_pixel.x, half_pixel.y), 0.0f, 0.0f))
            + 2.0f*tex2Dlod(back_buffer, float4(input.uv + float2(-half_pixel.x, half_pixel.y), 0.0f, 0.0f))
	    + 2.0f*tex2Dlod(back_buffer, float4(input.uv + float2(half_pixel.x, -half_pixel.y), 0.0f, 0.0f))
	    + 2.0f*tex2Dlod(back_buffer, float4(input.uv + float2(half_pixel.x, -half_pixel.y), 0.0f, 0.0f))) / 12.0f;
}
