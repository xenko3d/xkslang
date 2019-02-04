#version 410
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

struct ShaderMain_Streams
{
    vec4 s1;
    vec4 s2;
    int b1;
    float b2;
    int _unused;
};

struct PS_STREAMS
{
    vec4 s1_id0;
    vec4 s2_id1;
    int b1_id2;
    float b2_id3;
};

in vec4 PS_IN_S1;
in vec4 PS_IN_S2;
in int PS_IN_B1;
in float PS_IN_B2;

ShaderMain_Streams ShaderMain__getStreams_PS(PS_STREAMS _streams)
{
    ShaderMain_Streams res = ShaderMain_Streams(_streams.s1_id0, _streams.s2_id1, _streams.b1_id2, _streams.b2_id3, 0);
    return res;
}

void ShaderMain__setStreams_PS(inout PS_STREAMS _streams, ShaderMain_Streams _s)
{
    _streams.s1_id0 = _s.s1;
    _streams.s2_id1 = _s.s2;
    _streams.b1_id2 = _s.b1;
    _streams.b2_id3 = _s.b2;
}

void main()
{
    PS_STREAMS _streams = PS_STREAMS(vec4(0.0), vec4(0.0), 0, 0.0);
    _streams.s1_id0 = PS_IN_S1;
    _streams.s2_id1 = PS_IN_S2;
    _streams.b1_id2 = PS_IN_B1;
    _streams.b2_id3 = PS_IN_B2;
    ShaderMain_Streams s1 = ShaderMain__getStreams_PS(_streams);
    ShaderMain_Streams s3 = s1;
    ShaderMain_Streams s6 = ShaderMain__getStreams_PS(_streams);
    ShaderMain_Streams s2 = s3;
    ShaderMain_Streams s4 = ShaderMain__getStreams_PS(_streams);
    ShaderMain_Streams param = s4;
    ShaderMain__setStreams_PS(_streams, param);
}

