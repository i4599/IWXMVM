struct PS_INPUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

sampler2D back_buffer : register(s0);
float2 half_pixel : register(c0);

float4 main(PS_INPUT input) : COLOR
{
    return (4.0f * tex2Dlod(back_buffer, float4(input.uv, 0.0f, 0.0f))
            +      tex2Dlod(back_buffer, float4(input.uv - half_pixel, 0.0f, 0.0f))
	    +      tex2Dlod(back_buffer, float4(input.uv + half_pixel, 0.0f, 0.0f))
	    +      tex2Dlod(back_buffer, float4(input.uv + float2(half_pixel.x, -half_pixel.y), 0.0f, 0.0f))
	    +      tex2Dlod(back_buffer, float4(input.uv - float2(half_pixel.x, -half_pixel.y), 0.0f ,0.0f))) / 8.0f;
}
