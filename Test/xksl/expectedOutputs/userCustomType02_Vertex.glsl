#version 410
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

struct ShaderMain_DirectionalLightData
{
    vec3 DirectionWS;
    vec3 Color;
};

struct VS_STREAMS
{
    ShaderMain_DirectionalLightData aStreamVar_id0;
};

layout(std140) uniform aCBuffer
{
    ShaderMain_DirectionalLightData ShaderMain_toto;
} aCBuffer_var;

out ShaderMain_DirectionalLightData VS_OUT_aStreamVar;

void main()
{
    VS_STREAMS _streams = VS_STREAMS(ShaderMain_DirectionalLightData(vec3(0.0), vec3(0.0)));
    _streams.aStreamVar_id0.Color = aCBuffer_var.ShaderMain_toto.Color;
    VS_OUT_aStreamVar = _streams.aStreamVar_id0;
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
    gl_Position.y = -gl_Position.y;
}

