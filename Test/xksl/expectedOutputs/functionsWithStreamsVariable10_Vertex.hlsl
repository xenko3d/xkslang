struct TestShaderBase_Streams
{
    int sBase;
    int _unused;
};

struct TestShaderMain_Streams
{
    float3 sMain;
    int sBase;
    int _unused;
};

struct VS_STREAMS
{
    float3 sMain_id0;
    int sBase_id1;
};

static const VS_STREAMS _128 = { 0.0f.xxx, 0 };

static float3 VS_IN_SMAIN;
static int VS_IN_SBASE;
static float3 VS_OUT_sMain;
static int VS_OUT_sBase;

struct SPIRV_Cross_Input
{
    int VS_IN_SBASE : SBASE;
    float3 VS_IN_SMAIN : SMAIN;
};

struct SPIRV_Cross_Output
{
    int VS_OUT_sBase : SBASE;
    float3 VS_OUT_sMain : SMAIN;
};

TestShaderMain_Streams TestShaderMain__getStreams(VS_STREAMS _streams)
{
    TestShaderMain_Streams _60 = { _streams.sMain_id0, _streams.sBase_id1, 0 };
    TestShaderMain_Streams res = _60;
    return res;
}

TestShaderBase_Streams TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(TestShaderMain_Streams s)
{
    TestShaderBase_Streams _70 = { s.sBase, s._unused };
    TestShaderBase_Streams r = _70;
    return r;
}

void TestShaderBase_Compute(inout TestShaderBase_Streams s)
{
    s.sBase = 1;
}

void TestShaderMain_Compute(inout TestShaderMain_Streams s)
{
    s.sMain = 2.0f.xxx;
    TestShaderMain_Streams param = s;
    TestShaderBase_Streams param_1 = TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(param);
    TestShaderBase_Compute(param_1);
}

void TestShaderBase_Compute(int i, inout TestShaderBase_Streams s)
{
    s.sBase = i;
}

void TestShaderMain_Compute(int i, inout TestShaderMain_Streams s)
{
    s.sMain = 3.0f.xxx;
    TestShaderMain_Streams param = s;
    int param_1 = i;
    TestShaderBase_Streams param_2 = TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(param);
    TestShaderBase_Compute(param_1, param_2);
}

void TestShaderBase_Compute(inout TestShaderBase_Streams s1, TestShaderBase_Streams s2)
{
    s1.sBase = s2.sBase;
}

void TestShaderMain_Compute(inout TestShaderMain_Streams s1, TestShaderMain_Streams s2)
{
    s1.sMain = 4.0f.xxx;
    TestShaderMain_Streams param = s1;
    TestShaderMain_Streams param_1 = s2;
    TestShaderBase_Streams param_2 = TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(param);
    TestShaderBase_Streams param_3 = TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(param_1);
    TestShaderBase_Compute(param_2, param_3);
}

void vert_main()
{
    VS_STREAMS _streams = _128;
    _streams.sMain_id0 = VS_IN_SMAIN;
    _streams.sBase_id1 = VS_IN_SBASE;
    TestShaderMain_Streams backup = TestShaderMain__getStreams(_streams);
    TestShaderMain_Streams param = TestShaderMain__getStreams(_streams);
    TestShaderMain_Compute(param);
    int param_1 = 1;
    TestShaderMain_Streams param_2 = TestShaderMain__getStreams(_streams);
    TestShaderMain_Compute(param_1, param_2);
    TestShaderMain_Streams param_3 = TestShaderMain__getStreams(_streams);
    TestShaderMain_Streams param_4 = backup;
    TestShaderMain_Compute(param_3, param_4);
    VS_OUT_sMain = _streams.sMain_id0;
    VS_OUT_sBase = _streams.sBase_id1;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    VS_IN_SMAIN = stage_input.VS_IN_SMAIN;
    VS_IN_SBASE = stage_input.VS_IN_SBASE;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.VS_OUT_sMain = VS_OUT_sMain;
    stage_output.VS_OUT_sBase = VS_OUT_sBase;
    return stage_output;
}
