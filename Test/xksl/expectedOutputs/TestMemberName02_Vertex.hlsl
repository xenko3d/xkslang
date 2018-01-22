struct VS_STREAMS
{
    float4 s_in_id0;
    float4 s_int_id1;
};

cbuffer UpdatedGroupName
{
    float4 ShaderMain_ShadowMapTextureSize;
    float4 ShaderMain_ShadowMapTextureTexelSize;
    float4 ShaderBase_ShadowMapTextureSizeBase;
};
Texture2D<float4> ShaderMain_ShadowMapTexture;
SamplerState ShaderMain_Sampler0;

static float4 VS_IN_s_in;
static float4 VS_OUT_s_int;

struct SPIRV_Cross_Input
{
    float4 VS_IN_s_in : S_INPUT;
};

struct SPIRV_Cross_Output
{
    float4 VS_OUT_s_int : S_INT;
};

void vert_main()
{
    VS_STREAMS _streams = { 0.0f.xxxx, 0.0f.xxxx };
    _streams.s_in_id0 = VS_IN_s_in;
    float4 color = ShaderMain_ShadowMapTexture.Sample(ShaderMain_Sampler0, 0.0f.xx);
    _streams.s_int_id1 = (_streams.s_in_id0 + ShaderMain_ShadowMapTextureSize) + ShaderBase_ShadowMapTextureSizeBase;
    VS_OUT_s_int = _streams.s_int_id1;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    VS_IN_s_in = stage_input.VS_IN_s_in;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.VS_OUT_s_int = VS_OUT_s_int;
    return stage_output;
}
