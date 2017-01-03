
SamplerState Sampler01;
Texture2D Texture01;
float2 TexCoord;

SamplerState LinearSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
};

float4 Shading()
{
	float2 uv2 = TexCoord.xy;
	float4 color = Texture01.Sample(Sampler01, uv2);
	
	return color;
}