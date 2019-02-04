struct TestShaderBaseA_Streams
{
    float3 sBaseA;
    int _unused;
};

struct TestShaderBaseB_Streams
{
    float3 sBaseB;
    float3 sBaseA;
    int _unused;
};

struct TestShaderMain_Streams
{
    float3 sMain;
    float3 sBaseB;
    float3 sBaseA;
    int _unused;
};

struct VS_STREAMS
{
    float3 sMain_id0;
    float3 sBaseB_id1;
    float3 sBaseA_id2;
};

static const VS_STREAMS _100 = { 0.0f.xxx, 0.0f.xxx, 0.0f.xxx };

static float3 VS_IN_SMAIN;
static float3 VS_IN_SBASEB;
static float3 VS_IN_SBASEA;
static float3 VS_OUT_sMain;
static float3 VS_OUT_sBaseB;
static float3 VS_OUT_sBaseA;

struct SPIRV_Cross_Input
{
    float3 VS_IN_SBASEA : SBASEA;
    float3 VS_IN_SBASEB : SBASEB;
    float3 VS_IN_SMAIN : SMAIN;
};

struct SPIRV_Cross_Output
{
    float3 VS_OUT_sBaseA : SBASEA;
    float3 VS_OUT_sBaseB : SBASEB;
    float3 VS_OUT_sMain : SMAIN;
};

TestShaderMain_Streams TestShaderMain__getStreams(VS_STREAMS _streams)
{
    TestShaderMain_Streams _25 = { _streams.sMain_id0, _streams.sBaseB_id1, _streams.sBaseA_id2, 0 };
    TestShaderMain_Streams res = _25;
    return res;
}

TestShaderBaseB_Streams TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseBStreams(TestShaderMain_Streams s)
{
    TestShaderBaseB_Streams _37 = { s.sBaseB, s.sBaseA, s._unused };
    TestShaderBaseB_Streams r = _37;
    return r;
}

TestShaderBaseA_Streams TestShaderBaseB__ConvertTestShaderBaseBStreamsToTestShaderBaseAStreams(TestShaderBaseB_Streams s)
{
    TestShaderBaseA_Streams _60 = { s.sBaseA, s._unused };
    TestShaderBaseA_Streams r = _60;
    return r;
}

void TestShaderBaseA_Compute(inout TestShaderBaseA_Streams s)
{
    s.sBaseA = 1.0f.xxx;
}

void TestShaderBaseB_Compute(inout TestShaderBaseB_Streams s)
{
    s.sBaseB = 2.0f.xxx;
    TestShaderBaseB_Streams param = s;
    TestShaderBaseA_Streams param_1 = TestShaderBaseB__ConvertTestShaderBaseBStreamsToTestShaderBaseAStreams(param);
    TestShaderBaseA_Compute(param_1);
}

void TestShaderMain_Compute(inout TestShaderMain_Streams s)
{
    s.sMain = 1.0f.xxx;
    TestShaderMain_Streams param = s;
    TestShaderBaseB_Streams param_1 = TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseBStreams(param);
    TestShaderBaseB_Compute(param_1);
}

void vert_main()
{
    VS_STREAMS _streams = _100;
    _streams.sMain_id0 = VS_IN_SMAIN;
    _streams.sBaseB_id1 = VS_IN_SBASEB;
    _streams.sBaseA_id2 = VS_IN_SBASEA;
    TestShaderMain_Streams param = TestShaderMain__getStreams(_streams);
    TestShaderMain_Compute(param);
    VS_OUT_sMain = _streams.sMain_id0;
    VS_OUT_sBaseB = _streams.sBaseB_id1;
    VS_OUT_sBaseA = _streams.sBaseA_id2;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    VS_IN_SMAIN = stage_input.VS_IN_SMAIN;
    VS_IN_SBASEB = stage_input.VS_IN_SBASEB;
    VS_IN_SBASEA = stage_input.VS_IN_SBASEA;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.VS_OUT_sMain = VS_OUT_sMain;
    stage_output.VS_OUT_sBaseB = VS_OUT_sBaseB;
    stage_output.VS_OUT_sBaseA = VS_OUT_sBaseA;
    return stage_output;
}
