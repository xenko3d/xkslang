struct VS_STREAMS
{
    int aStream_id0;
};

cbuffer PreDraw
{
    int ShaderBase_4_1__aVar;
};

static int VS_OUT_aStream;

struct SPIRV_Cross_Output
{
    int VS_OUT_aStream : TEXCOORD0;
};

int ShaderBase_4_1__compute()
{
    return ShaderBase_4_1__aVar + 4;
}

int ShaderMain_7_4__compute()
{
    return ShaderBase_4_1__compute();
}

void vert_main()
{
    VS_STREAMS _streams = { 0 };
    _streams.aStream_id0 = ShaderMain_7_4__compute() + 11;
    VS_OUT_aStream = _streams.aStream_id0;
}

SPIRV_Cross_Output main()
{
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.VS_OUT_aStream = VS_OUT_aStream;
    return stage_output;
}
