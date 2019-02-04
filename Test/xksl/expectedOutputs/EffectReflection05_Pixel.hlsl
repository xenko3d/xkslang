struct ShaderMain_StructType
{
    float4 aF4;
    float4 ColorArray[2];
    uint aBool;
    float4 ColorArrayB[2];
    float2x3 aMat23[2];
    float2x3 aMat23_rm[2];
};

struct PS_STREAMS
{
    float4 ColorTarget_id0;
};

static const PS_STREAMS _32 = { 0.0f.xxxx };

cbuffer Globals
{
    ShaderMain_StructType ShaderMain_var1;
    float4 ShaderMain_aCol;
};

static float4 PS_OUT_ColorTarget;

struct SPIRV_Cross_Output
{
    float4 PS_OUT_ColorTarget : SV_Target0;
};

void frag_main()
{
    PS_STREAMS _streams = _32;
    _streams.ColorTarget_id0 = ShaderMain_aCol + ShaderMain_var1.aF4;
    PS_OUT_ColorTarget = _streams.ColorTarget_id0;
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.PS_OUT_ColorTarget = PS_OUT_ColorTarget;
    return stage_output;
}
