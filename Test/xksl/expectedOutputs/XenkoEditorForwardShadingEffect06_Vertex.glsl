#version 410
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

struct LightDirectional_DirectionalLightData
{
    vec3 DirectionWS;
    vec3 Color;
};

struct VS_STREAMS
{
    vec4 ShadingPosition_id0;
    vec3 meshNormal_id1;
    vec4 meshTangent_id2;
    vec3 normalWS_id3;
    vec4 Position_id4;
    vec4 PositionWS_id5;
    float DepthVS_id6;
    vec4 PositionH_id7;
    vec2 TexCoord_id8;
    vec4 ScreenPosition_id9;
};

layout(std140) uniform PerDraw
{
    layout(row_major) mat4 Transformation_World;
    layout(row_major) mat4 Transformation_WorldInverse;
    layout(row_major) mat4 Transformation_WorldInverseTranspose;
    layout(row_major) mat4 Transformation_WorldView;
    layout(row_major) mat4 Transformation_WorldViewInverse;
    layout(row_major) mat4 Transformation_WorldViewProjection;
    vec3 Transformation_WorldScale;
    vec4 Transformation_EyeMS;
} PerDraw_var;

layout(std140) uniform PerView
{
    layout(row_major) mat4 Transformation_View;
    layout(row_major) mat4 Transformation_ViewInverse;
    layout(row_major) mat4 Transformation_Projection;
    layout(row_major) mat4 Transformation_ProjectionInverse;
    layout(row_major) mat4 Transformation_ViewProjection;
    vec2 Transformation_ProjScreenRay;
    vec4 Transformation_Eye;
    float o1S433C0_Camera_NearClipPlane;
    float o1S433C0_Camera_FarClipPlane;
    vec2 o1S433C0_Camera_ZProjection;
    vec2 o1S433C0_Camera_ViewSize;
    float o1S433C0_Camera_AspectRatio;
    vec4 o0S433C0_ShadowMapReceiverDirectional_padding_PerView_Default;
    float o0S433C0_ShadowMapReceiverDirectional_CascadeDepthSplits[4];
    layout(row_major) mat4 o0S433C0_ShadowMapReceiverBase_WorldToShadowCascadeUV[4];
    layout(row_major) mat4 o0S433C0_ShadowMapReceiverBase_InverseWorldToShadowCascadeUV[4];
    layout(row_major) mat4 o0S433C0_ShadowMapReceiverBase_ViewMatrices[4];
    vec2 o0S433C0_ShadowMapReceiverBase_DepthRanges[4];
    float o0S433C0_ShadowMapReceiverBase_DepthBiases[1];
    float o0S433C0_ShadowMapReceiverBase_OffsetScales[1];
    vec2 o0S433C0_ShadowMapCommon_ShadowMapTextureSize;
    vec2 o0S433C0_ShadowMapCommon_ShadowMapTextureTexelSize;
    LightDirectional_DirectionalLightData o0S433C0_LightDirectionalGroup_Lights[1];
    int o0S433C0_DirectLightGroupPerView_LightCount;
    float o1S433C0_LightClustered_ClusterDepthScale;
    float o1S433C0_LightClustered_ClusterDepthBias;
    vec2 o1S433C0_LightClustered_ClusterStride;
    vec3 o3S417C0_LightSimpleAmbient_AmbientLight;
    layout(row_major) mat4 o6S417C0_LightSkyboxShader_SkyMatrix;
    float o6S417C0_LightSkyboxShader_Intensity;
    float o6S417C0_o5S5C1_RoughnessCubeMapEnvironmentColor_MipCount;
    vec3 o6S417C0_o4S5C0_SphericalHarmonicsEnvironmentColor_SphericalColors[9];
    vec4 o6S417C0_o4S5C0_SphericalHarmonicsEnvironmentColor_padding_PerView_Lighting;
} PerView_var;

in vec3 VS_IN_NORMAL;
in vec4 VS_IN_TANGENT;
in vec4 VS_IN_POSITION;
in vec2 VS_IN_TEXCOORD0;
out vec3 VS_OUT_meshNormal;
out vec4 VS_OUT_meshTangent;
out vec4 VS_OUT_PositionWS;
out float VS_OUT_DepthVS;
out vec2 VS_OUT_TexCoord;
out vec4 VS_OUT_ScreenPosition;

void ShaderBase_VSMain()
{
}

void TransformationBase_PreTransformPosition()
{
}

void TransformationWAndVP_PreTransformPosition(inout VS_STREAMS _streams)
{
    TransformationBase_PreTransformPosition();
    _streams.PositionWS_id5 = PerDraw_var.Transformation_World * _streams.Position_id4;
}

void TransformationBase_TransformPosition()
{
}

void TransformationBase_PostTransformPosition()
{
}

vec4 TransformationWAndVP_ComputeShadingPosition(vec4 world)
{
    return PerView_var.Transformation_ViewProjection * world;
}

void TransformationWAndVP_PostTransformPosition(inout VS_STREAMS _streams)
{
    TransformationBase_PostTransformPosition();
    vec4 param = _streams.PositionWS_id5;
    _streams.ShadingPosition_id0 = TransformationWAndVP_ComputeShadingPosition(param);
    _streams.PositionH_id7 = _streams.ShadingPosition_id0;
    _streams.DepthVS_id6 = _streams.ShadingPosition_id0.w;
}

void TransformationBase_BaseTransformVS(inout VS_STREAMS _streams)
{
    TransformationWAndVP_PreTransformPosition(_streams);
    TransformationBase_TransformPosition();
    TransformationWAndVP_PostTransformPosition(_streams);
}

void TransformationBase_VSMain(inout VS_STREAMS _streams)
{
    ShaderBase_VSMain();
    TransformationBase_BaseTransformVS(_streams);
}

void NormalUpdate_GenerateNormal_VS(inout VS_STREAMS _streams)
{
    _streams.normalWS_id3 = vec3(0.0);
}

void NormalBase_VSMain(inout VS_STREAMS _streams)
{
    TransformationBase_VSMain(_streams);
    NormalUpdate_GenerateNormal_VS(_streams);
}

void main()
{
    VS_STREAMS _streams = VS_STREAMS(vec4(0.0), vec3(0.0), vec4(0.0), vec3(0.0), vec4(0.0), vec4(0.0), 0.0, vec4(0.0), vec2(0.0), vec4(0.0));
    _streams.meshNormal_id1 = VS_IN_NORMAL;
    _streams.meshTangent_id2 = VS_IN_TANGENT;
    _streams.Position_id4 = VS_IN_POSITION;
    _streams.TexCoord_id8 = VS_IN_TEXCOORD0;
    NormalBase_VSMain(_streams);
    _streams.ScreenPosition_id9 = _streams.ShadingPosition_id0;
    gl_Position = _streams.ShadingPosition_id0;
    VS_OUT_meshNormal = _streams.meshNormal_id1;
    VS_OUT_meshTangent = _streams.meshTangent_id2;
    VS_OUT_PositionWS = _streams.PositionWS_id5;
    VS_OUT_DepthVS = _streams.DepthVS_id6;
    VS_OUT_TexCoord = _streams.TexCoord_id8;
    VS_OUT_ScreenPosition = _streams.ScreenPosition_id9;
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
    gl_Position.y = -gl_Position.y;
}

