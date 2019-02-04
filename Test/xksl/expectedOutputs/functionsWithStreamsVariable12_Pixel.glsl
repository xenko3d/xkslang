#version 410
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

struct TestIComposition_Streams
{
    vec3 sComp;
    int _unused;
};

struct TestShaderMain_Streams
{
    vec3 sMain;
    vec3 sBase;
    int _unused;
};

struct PS_STREAMS
{
    vec3 sComp_id0;
    vec3 sMain_id1;
    vec3 sBase_id2;
};

in vec3 PS_IN_SMAIN;
in vec3 PS_IN_SBASE;

TestShaderMain_Streams TestShaderMain__getStreams_PS(PS_STREAMS _streams)
{
    TestShaderMain_Streams res = TestShaderMain_Streams(_streams.sMain_id1, _streams.sBase_id2, 0);
    return res;
}

TestIComposition_Streams TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(TestShaderMain_Streams s)
{
    TestIComposition_Streams r = TestIComposition_Streams(s.sBase, s._unused);
    return r;
}

void TestShaderBase_Compute(inout TestIComposition_Streams s)
{
    s.sComp = vec3(1.0);
}

TestIComposition_Streams TestShaderMain__ConvertTestShaderMainStreamsToTestICompositionStreams(TestShaderMain_Streams s)
{
    TestIComposition_Streams r = TestIComposition_Streams(vec3(0.0), s._unused);
    return r;
}

void o0S5C0_TestIComposition_ComputeComp_PS(inout PS_STREAMS _streams, TestIComposition_Streams s)
{
    _streams.sComp_id0 = s.sComp;
}

void TestShaderMain_Compute_PS(inout PS_STREAMS _streams, inout TestShaderMain_Streams s)
{
    s.sMain = vec3(2.0);
    TestShaderMain_Streams param = s;
    TestIComposition_Streams param_1 = TestShaderMain__ConvertTestShaderMainStreamsToTestShaderBaseStreams(param);
    TestShaderBase_Compute(param_1);
    TestShaderMain_Streams backup = TestShaderMain__getStreams_PS(_streams);
    TestShaderMain_Streams param_2 = backup;
    TestIComposition_Streams param_3 = TestShaderMain__ConvertTestShaderMainStreamsToTestICompositionStreams(param_2);
    o0S5C0_TestIComposition_ComputeComp_PS(_streams, param_3);
}

void main()
{
    PS_STREAMS _streams = PS_STREAMS(vec3(0.0), vec3(0.0), vec3(0.0));
    _streams.sMain_id1 = PS_IN_SMAIN;
    _streams.sBase_id2 = PS_IN_SBASE;
    TestShaderMain_Streams param = TestShaderMain__getStreams_PS(_streams);
    TestShaderMain_Compute_PS(_streams, param);
}

