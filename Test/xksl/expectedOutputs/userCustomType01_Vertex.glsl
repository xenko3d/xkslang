#version 450

struct ShaderMain_DirectionalLightData
{
    vec3 DirectionWS;
    vec3 Color;
};

struct VS_STREAMS
{
    ShaderMain_DirectionalLightData aStreamVar_id0;
};

layout(location = 0) out ShaderMain_DirectionalLightData VS_OUT_aStreamVar;

void main()
{
    VS_STREAMS _streams = VS_STREAMS(ShaderMain_DirectionalLightData(vec3(0.0), vec3(0.0)));
    _streams.aStreamVar_id0.Color.x = 5.0;
    VS_OUT_aStreamVar = _streams.aStreamVar_id0;
}

