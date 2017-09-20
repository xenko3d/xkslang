#version 450

struct LightDirectional_DirectionalLightData
{
    vec3 DirectionWS;
    vec3 Color;
};

struct PS_STREAMS
{
    vec4 ShadingPosition_id0;
    bool IsFrontFace_id1;
    vec4 ColorTarget_id2;
    float matBlend_id3;
    vec3 meshNormal_id4;
    vec4 meshTangent_id5;
    vec3 normalWS_id6;
    mat3 tangentToWorld_id7;
    vec4 PositionWS_id8;
    vec3 matNormal_id9;
    vec4 matColorBase_id10;
    vec4 matDiffuse_id11;
    float matGlossiness_id12;
    vec3 matSpecular_id13;
    float matSpecularIntensity_id14;
    float matAmbientOcclusion_id15;
    float matAmbientOcclusionDirectLightingFactor_id16;
    float matCavity_id17;
    float matCavityDiffuse_id18;
    float matCavitySpecular_id19;
    vec4 matEmissive_id20;
    float matEmissiveIntensity_id21;
    vec2 matDiffuseSpecularAlphaBlend_id22;
    vec3 matAlphaBlendColor_id23;
    float matAlphaDiscard_id24;
    vec3 viewWS_id25;
    vec3 matDiffuseVisible_id26;
    float alphaRoughness_id27;
    vec3 matSpecularVisible_id28;
    float NdotV_id29;
    vec3 shadingColor_id30;
    float shadingColorAlpha_id31;
    vec3 H_id32;
    float NdotH_id33;
    float LdotH_id34;
    float VdotH_id35;
    vec3 lightPositionWS_id36;
    vec3 lightDirectionWS_id37;
    vec3 lightColor_id38;
    vec3 lightColorNdotL_id39;
    vec3 envLightDiffuseColor_id40;
    vec3 envLightSpecularColor_id41;
    float NdotL_id42;
    float lightDirectAmbientOcclusion_id43;
    vec3 shadowColor_id44;
    vec2 TexCoord_id45;
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
    vec4 o0S418C0_LightDirectionalGroup__padding_PerView_Default;
    LightDirectional_DirectionalLightData o0S418C0_LightDirectionalGroup_Lights[8];
    int o0S418C0_DirectLightGroupPerView_LightCount;
    vec3 o1S403C0_LightSimpleAmbient_AmbientLight;
    vec4 o1S403C0_LightSimpleAmbient__padding_PerView_Lighting;
} PerView_var;

layout(std140) uniform PerMaterial
{
    float o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_constantFloat;
    vec4 o19S248C0_o9S2C0_o8S2C0_ComputeColorConstantColorLink_constantColor;
    float o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_constantFloat;
    vec2 o19S248C0_o3S2C0_o2S2C0_ComputeColorTextureScaledOffsetDynamicSampler_scale;
    vec2 o19S248C0_o3S2C0_o2S2C0_ComputeColorTextureScaledOffsetDynamicSampler_offset;
    float o25S35C0_o23S2C0_o22S2C0_o21S2C1_ComputeColorConstantFloatLink_constantFloat;
} PerMaterial_var;

layout(std140) uniform PerFrame
{
    float Global_Time;
    float Global_TimeStep;
} PerFrame_var;

uniform sampler2D SPIRV_Cross_CombinedDynamicTexture_TextureDynamicSampler_Sampler;
uniform sampler2D SPIRV_Cross_CombinedMaterialSpecularMicrofacetEnvironmentGGXLUT_EnvironmentLightingDFG_LUTTexturing_LinearSampler;

layout(location = 0) in vec4 PS_IN_ShadingPosition;
layout(location = 1) in vec3 PS_IN_meshNormal;
layout(location = 2) in vec4 PS_IN_meshTangent;
layout(location = 3) in vec4 PS_IN_PositionWS;
layout(location = 4) in vec2 PS_IN_TexCoord;
layout(location = 5) in bool PS_IN_IsFrontFace;
layout(location = 0) out vec4 PS_OUT_ColorTarget;

void NormalUpdate_GenerateNormal_PS()
{
}

mat3 NormalUpdate_GetTangentMatrix(inout PS_STREAMS _streams)
{
    _streams.meshNormal_id4 = normalize(_streams.meshNormal_id4);
    vec3 tangent = normalize(_streams.meshTangent_id5.xyz);
    vec3 bitangent = cross(_streams.meshNormal_id4, tangent) * _streams.meshTangent_id5.w;
    mat3 tangentMatrix = mat3(vec3(tangent), vec3(bitangent), vec3(_streams.meshNormal_id4));
    return tangentMatrix;
}

mat3 NormalFromNormalMapping_GetTangentWorldTransform()
{
    return mat3(vec3(PerDraw_var.Transformation_WorldInverseTranspose[0].x, PerDraw_var.Transformation_WorldInverseTranspose[0].y, PerDraw_var.Transformation_WorldInverseTranspose[0].z), vec3(PerDraw_var.Transformation_WorldInverseTranspose[1].x, PerDraw_var.Transformation_WorldInverseTranspose[1].y, PerDraw_var.Transformation_WorldInverseTranspose[1].z), vec3(PerDraw_var.Transformation_WorldInverseTranspose[2].x, PerDraw_var.Transformation_WorldInverseTranspose[2].y, PerDraw_var.Transformation_WorldInverseTranspose[2].z));
}

void NormalUpdate_UpdateTangentToWorld(inout PS_STREAMS _streams)
{
    mat3 _95 = NormalUpdate_GetTangentMatrix(_streams);
    mat3 tangentMatrix = _95;
    mat3 tangentWorldTransform = NormalFromNormalMapping_GetTangentWorldTransform();
    _streams.tangentToWorld_id7 = tangentWorldTransform * tangentMatrix;
}

void NormalFromNormalMapping_GenerateNormal_PS(inout PS_STREAMS _streams)
{
    NormalUpdate_GenerateNormal_PS();
    NormalUpdate_UpdateTangentToWorld(_streams);
}

void ShaderBase_PSMain()
{
}

void o26S248C1_IStreamInitializer_ResetStream()
{
}

void o26S248C1_MaterialStream_ResetStream(inout PS_STREAMS _streams)
{
    o26S248C1_IStreamInitializer_ResetStream();
    _streams.matBlend_id3 = 0.0;
}

void o26S248C1_MaterialPixelStream_ResetStream(inout PS_STREAMS _streams)
{
    o26S248C1_MaterialStream_ResetStream(_streams);
    _streams.matNormal_id9 = vec3(0.0, 0.0, 1.0);
    _streams.matColorBase_id10 = vec4(0.0);
    _streams.matDiffuse_id11 = vec4(0.0);
    _streams.matDiffuseVisible_id26 = vec3(0.0);
    _streams.matSpecular_id13 = vec3(0.0);
    _streams.matSpecularVisible_id28 = vec3(0.0);
    _streams.matSpecularIntensity_id14 = 1.0;
    _streams.matGlossiness_id12 = 0.0;
    _streams.alphaRoughness_id27 = 1.0;
    _streams.matAmbientOcclusion_id15 = 1.0;
    _streams.matAmbientOcclusionDirectLightingFactor_id16 = 0.0;
    _streams.matCavity_id17 = 1.0;
    _streams.matCavityDiffuse_id18 = 0.0;
    _streams.matCavitySpecular_id19 = 0.0;
    _streams.matEmissive_id20 = vec4(0.0);
    _streams.matEmissiveIntensity_id21 = 0.0;
    _streams.matDiffuseSpecularAlphaBlend_id22 = vec2(1.0);
    _streams.matAlphaBlendColor_id23 = vec3(1.0);
    _streams.matAlphaDiscard_id24 = 0.100000001490116119384765625;
}

void o26S248C1_MaterialPixelShadingStream_ResetStream(inout PS_STREAMS _streams)
{
    o26S248C1_MaterialPixelStream_ResetStream(_streams);
    _streams.shadingColorAlpha_id31 = 1.0;
}

vec4 o19S248C0_o3S2C0_o2S2C0_ComputeColorTextureScaledOffsetDynamicSampler_Material_DiffuseMap_TEXCOORD0_Material_Sampler_i0_rgba_Material_TextureScale_Material_TextureOffset__Compute(PS_STREAMS _streams)
{
    return texture(SPIRV_Cross_CombinedDynamicTexture_TextureDynamicSampler_Sampler, (_streams.TexCoord_id45 * PerMaterial_var.o19S248C0_o3S2C0_o2S2C0_ComputeColorTextureScaledOffsetDynamicSampler_scale) + PerMaterial_var.o19S248C0_o3S2C0_o2S2C0_ComputeColorTextureScaledOffsetDynamicSampler_offset);
}

void o19S248C0_o3S2C0_MaterialSurfaceDiffuse_Compute(inout PS_STREAMS _streams)
{
    vec4 colorBase = o19S248C0_o3S2C0_o2S2C0_ComputeColorTextureScaledOffsetDynamicSampler_Material_DiffuseMap_TEXCOORD0_Material_Sampler_i0_rgba_Material_TextureScale_Material_TextureOffset__Compute(_streams);
    _streams.matDiffuse_id11 = colorBase;
    _streams.matColorBase_id10 = colorBase;
}

vec2 o19S248C0_o5S2C0_o4S2C0_ComputeColorWaveNormal_5_0_1__0_03__SincosOfAtan(float x)
{
    return vec2(x, 1.0) / vec2(sqrt(1.0 + (x * x)));
}

vec4 o19S248C0_o5S2C0_o4S2C0_ComputeColorWaveNormal_5_0_1__0_03__Compute(PS_STREAMS _streams)
{
    vec2 offset = _streams.TexCoord_id45 - vec2(0.5);
    float phase = length(offset);
    float derivative = cos((((phase + (PerFrame_var.Global_Time * (-0.02999999932944774627685546875))) * 2.0) * 3.1400001049041748046875) * 5.0) * 0.100000001490116119384765625;
    float param = offset.y / offset.x;
    vec2 xz = o19S248C0_o5S2C0_o4S2C0_ComputeColorWaveNormal_5_0_1__0_03__SincosOfAtan(param);
    float param_1 = derivative;
    vec2 xy = o19S248C0_o5S2C0_o4S2C0_ComputeColorWaveNormal_5_0_1__0_03__SincosOfAtan(param_1);
    vec2 _724 = (((xz.yx * sign(offset.x)) * (-xy.x)) * 0.5) + vec2(0.5);
    vec3 normal;
    normal = vec3(_724.x, _724.y, normal.z);
    normal.z = xy.y;
    return vec4(normal, 1.0);
}

void o19S248C0_o5S2C0_MaterialSurfaceNormalMap_false_true__Compute(inout PS_STREAMS _streams)
{
    vec4 normal = o19S248C0_o5S2C0_o4S2C0_ComputeColorWaveNormal_5_0_1__0_03__Compute(_streams);
    if (true)
    {
        normal = (normal * 2.0) - vec4(1.0);
    }
    if (false)
    {
        normal.z = sqrt(max(0.0, 1.0 - ((normal.x * normal.x) + (normal.y * normal.y))));
    }
    _streams.matNormal_id9 = normal.xyz;
}

vec4 o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_Material_GlossinessValue__Compute()
{
    return vec4(PerMaterial_var.o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_constantFloat, PerMaterial_var.o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_constantFloat, PerMaterial_var.o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_constantFloat, PerMaterial_var.o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_constantFloat);
}

void o19S248C0_o7S2C0_MaterialSurfaceGlossinessMap_false__Compute(inout PS_STREAMS _streams)
{
    float glossiness = o19S248C0_o7S2C0_o6S2C0_ComputeColorConstantFloatLink_Material_GlossinessValue__Compute().x;
    if (false)
    {
        glossiness = 1.0 - glossiness;
    }
    _streams.matGlossiness_id12 = glossiness;
}

vec4 o19S248C0_o9S2C0_o8S2C0_ComputeColorConstantColorLink_Material_SpecularValue__Compute()
{
    return PerMaterial_var.o19S248C0_o9S2C0_o8S2C0_ComputeColorConstantColorLink_constantColor;
}

void o19S248C0_o9S2C0_MaterialSurfaceSetStreamFromComputeColor_matSpecular_rgb__Compute(inout PS_STREAMS _streams)
{
    _streams.matSpecular_id13 = o19S248C0_o9S2C0_o8S2C0_ComputeColorConstantColorLink_Material_SpecularValue__Compute().xyz;
}

vec4 o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_Material_SpecularIntensityValue__Compute()
{
    return vec4(PerMaterial_var.o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_constantFloat, PerMaterial_var.o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_constantFloat, PerMaterial_var.o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_constantFloat, PerMaterial_var.o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_constantFloat);
}

void o19S248C0_o11S2C0_MaterialSurfaceSetStreamFromComputeColor_matSpecularIntensity_r__Compute(inout PS_STREAMS _streams)
{
    _streams.matSpecularIntensity_id14 = o19S248C0_o11S2C0_o10S2C0_ComputeColorConstantFloatLink_Material_SpecularIntensityValue__Compute().x;
}

void NormalUpdate_UpdateNormalFromTangentSpace(inout PS_STREAMS _streams, vec3 normalInTangentSpace)
{
    _streams.normalWS_id6 = normalize(_streams.tangentToWorld_id7 * normalInTangentSpace);
}

void o19S248C0_o18S2C0_LightStream_ResetLightStream(inout PS_STREAMS _streams)
{
    _streams.lightPositionWS_id36 = vec3(0.0);
    _streams.lightDirectionWS_id37 = vec3(0.0);
    _streams.lightColor_id38 = vec3(0.0);
    _streams.lightColorNdotL_id39 = vec3(0.0);
    _streams.envLightDiffuseColor_id40 = vec3(0.0);
    _streams.envLightSpecularColor_id41 = vec3(0.0);
    _streams.lightDirectAmbientOcclusion_id43 = 1.0;
    _streams.NdotL_id42 = 0.0;
}

void o19S248C0_o18S2C0_MaterialPixelStream_PrepareMaterialForLightingAndShading(inout PS_STREAMS _streams)
{
    _streams.lightDirectAmbientOcclusion_id43 = mix(1.0, _streams.matAmbientOcclusion_id15, _streams.matAmbientOcclusionDirectLightingFactor_id16);
    _streams.matDiffuseVisible_id26 = ((_streams.matDiffuse_id11.xyz * mix(1.0, _streams.matCavity_id17, _streams.matCavityDiffuse_id18)) * _streams.matDiffuseSpecularAlphaBlend_id22.x) * _streams.matAlphaBlendColor_id23;
    _streams.matSpecularVisible_id28 = (((_streams.matSpecular_id13 * _streams.matSpecularIntensity_id14) * mix(1.0, _streams.matCavity_id17, _streams.matCavitySpecular_id19)) * _streams.matDiffuseSpecularAlphaBlend_id22.y) * _streams.matAlphaBlendColor_id23;
    _streams.NdotV_id29 = max(dot(_streams.normalWS_id6, _streams.viewWS_id25), 9.9999997473787516355514526367188e-05);
    float roughness = 1.0 - _streams.matGlossiness_id12;
    _streams.alphaRoughness_id27 = max(roughness * roughness, 0.001000000047497451305389404296875);
}

void o0S418C0_DirectLightGroup_PrepareDirectLights()
{
}

int o0S418C0_LightDirectionalGroup_8__GetMaxLightCount()
{
    return 8;
}

int o0S418C0_DirectLightGroupPerView_GetLightCount()
{
    return PerView_var.o0S418C0_DirectLightGroupPerView_LightCount;
}

void o0S418C0_LightDirectionalGroup_8__PrepareDirectLightCore(inout PS_STREAMS _streams, int lightIndex)
{
    _streams.lightColor_id38 = PerView_var.o0S418C0_LightDirectionalGroup_Lights[lightIndex].Color;
    _streams.lightDirectionWS_id37 = -PerView_var.o0S418C0_LightDirectionalGroup_Lights[lightIndex].DirectionWS;
}

vec3 o0S418C0_ShadowGroup_ComputeShadow(vec3 position, int lightIndex)
{
    return vec3(1.0);
}

void o0S418C0_DirectLightGroup_PrepareDirectLight(inout PS_STREAMS _streams, int lightIndex)
{
    int param = lightIndex;
    o0S418C0_LightDirectionalGroup_8__PrepareDirectLightCore(_streams, param);
    _streams.NdotL_id42 = max(dot(_streams.normalWS_id6, _streams.lightDirectionWS_id37), 9.9999997473787516355514526367188e-05);
    vec3 param_1 = _streams.PositionWS_id8.xyz;
    int param_2 = lightIndex;
    _streams.shadowColor_id44 = o0S418C0_ShadowGroup_ComputeShadow(param_1, param_2);
    _streams.lightColorNdotL_id39 = ((_streams.lightColor_id38 * _streams.shadowColor_id44) * _streams.NdotL_id42) * _streams.lightDirectAmbientOcclusion_id43;
}

void MaterialPixelShadingStream_PrepareMaterialPerDirectLight(inout PS_STREAMS _streams)
{
    _streams.H_id32 = normalize(_streams.viewWS_id25 + _streams.lightDirectionWS_id37);
    _streams.NdotH_id33 = clamp(dot(_streams.normalWS_id6, _streams.H_id32), 0.0, 1.0);
    _streams.LdotH_id34 = clamp(dot(_streams.lightDirectionWS_id37, _streams.H_id32), 0.0, 1.0);
    _streams.VdotH_id35 = _streams.LdotH_id34;
}

vec3 o19S248C0_o18S2C0_o12S2C0_MaterialSurfaceShadingDiffuseLambert_true__ComputeDirectLightContribution(PS_STREAMS _streams)
{
    vec3 diffuseColor = _streams.matDiffuseVisible_id26;
    if (true)
    {
        diffuseColor *= (vec3(1.0) - _streams.matSpecularVisible_id28);
    }
    return ((diffuseColor / vec3(3.1415927410125732421875)) * _streams.lightColorNdotL_id39) * _streams.matDiffuseSpecularAlphaBlend_id22.x;
}

vec3 o19S248C0_o18S2C0_o17S2C0_o14S2C0_BRDFMicrofacet_FresnelSchlick(vec3 f0, vec3 f90, float lOrVDotH)
{
    return f0 + ((f90 - f0) * pow(1.0 - lOrVDotH, 5.0));
}

vec3 o19S248C0_o18S2C0_o17S2C0_o14S2C0_BRDFMicrofacet_FresnelSchlick(vec3 f0, float lOrVDotH)
{
    vec3 param = f0;
    vec3 param_1 = vec3(1.0);
    float param_2 = lOrVDotH;
    return o19S248C0_o18S2C0_o17S2C0_o14S2C0_BRDFMicrofacet_FresnelSchlick(param, param_1, param_2);
}

vec3 o19S248C0_o18S2C0_o17S2C0_o14S2C0_MaterialSpecularMicrofacetFresnelSchlick_Compute(PS_STREAMS _streams, vec3 f0)
{
    vec3 param = f0;
    float param_1 = _streams.LdotH_id34;
    return o19S248C0_o18S2C0_o17S2C0_o14S2C0_BRDFMicrofacet_FresnelSchlick(param, param_1);
}

float o19S248C0_o18S2C0_o17S2C0_o15S2C1_BRDFMicrofacet_VisibilityhSchlickGGX(float alphaR, float nDotX)
{
    float k = alphaR * 0.5;
    return nDotX / ((nDotX * (1.0 - k)) + k);
}

float o19S248C0_o18S2C0_o17S2C0_o15S2C1_BRDFMicrofacet_VisibilitySmithSchlickGGX(float alphaR, float nDotL, float nDotV)
{
    float param = alphaR;
    float param_1 = nDotL;
    float param_2 = alphaR;
    float param_3 = nDotV;
    return (o19S248C0_o18S2C0_o17S2C0_o15S2C1_BRDFMicrofacet_VisibilityhSchlickGGX(param, param_1) * o19S248C0_o18S2C0_o17S2C0_o15S2C1_BRDFMicrofacet_VisibilityhSchlickGGX(param_2, param_3)) / (nDotL * nDotV);
}

float o19S248C0_o18S2C0_o17S2C0_o15S2C1_MaterialSpecularMicrofacetVisibilitySmithSchlickGGX_Compute(PS_STREAMS _streams)
{
    float param = _streams.alphaRoughness_id27;
    float param_1 = _streams.NdotL_id42;
    float param_2 = _streams.NdotV_id29;
    return o19S248C0_o18S2C0_o17S2C0_o15S2C1_BRDFMicrofacet_VisibilitySmithSchlickGGX(param, param_1, param_2);
}

float o19S248C0_o18S2C0_o17S2C0_o16S2C2_BRDFMicrofacet_NormalDistributionGGX(float alphaR, float nDotH)
{
    float alphaR2 = alphaR * alphaR;
    float d = max(((nDotH * nDotH) * (alphaR2 - 1.0)) + 1.0, 9.9999997473787516355514526367188e-05);
    return alphaR2 / ((3.1415927410125732421875 * d) * d);
}

float o19S248C0_o18S2C0_o17S2C0_o16S2C2_MaterialSpecularMicrofacetNormalDistributionGGX_Compute(PS_STREAMS _streams)
{
    float param = _streams.alphaRoughness_id27;
    float param_1 = _streams.NdotH_id33;
    return o19S248C0_o18S2C0_o17S2C0_o16S2C2_BRDFMicrofacet_NormalDistributionGGX(param, param_1);
}

vec3 o19S248C0_o18S2C0_o17S2C0_MaterialSurfaceShadingSpecularMicrofacet_ComputeDirectLightContribution(PS_STREAMS _streams)
{
    vec3 specularColor = _streams.matSpecularVisible_id28;
    vec3 param = specularColor;
    vec3 fresnel = o19S248C0_o18S2C0_o17S2C0_o14S2C0_MaterialSpecularMicrofacetFresnelSchlick_Compute(_streams, param);
    float geometricShadowing = o19S248C0_o18S2C0_o17S2C0_o15S2C1_MaterialSpecularMicrofacetVisibilitySmithSchlickGGX_Compute(_streams);
    float normalDistribution = o19S248C0_o18S2C0_o17S2C0_o16S2C2_MaterialSpecularMicrofacetNormalDistributionGGX_Compute(_streams);
    vec3 reflected = ((fresnel * geometricShadowing) * normalDistribution) / vec3(4.0);
    return (reflected * _streams.lightColorNdotL_id39) * _streams.matDiffuseSpecularAlphaBlend_id22.y;
}

void o1S403C0_EnvironmentLight_PrepareEnvironmentLight(inout PS_STREAMS _streams)
{
    _streams.envLightDiffuseColor_id40 = vec3(0.0);
    _streams.envLightSpecularColor_id41 = vec3(0.0);
}

void o1S403C0_LightSimpleAmbient_PrepareEnvironmentLight(inout PS_STREAMS _streams)
{
    o1S403C0_EnvironmentLight_PrepareEnvironmentLight(_streams);
    vec3 lightColor = PerView_var.o1S403C0_LightSimpleAmbient_AmbientLight * _streams.matAmbientOcclusion_id15;
    _streams.envLightDiffuseColor_id40 = lightColor;
    _streams.envLightSpecularColor_id41 = lightColor;
}

vec3 o19S248C0_o18S2C0_o12S2C0_MaterialSurfaceShadingDiffuseLambert_true__ComputeEnvironmentLightContribution(PS_STREAMS _streams)
{
    vec3 diffuseColor = _streams.matDiffuseVisible_id26;
    if (true)
    {
        diffuseColor *= (vec3(1.0) - _streams.matSpecularVisible_id28);
    }
    return diffuseColor * _streams.envLightDiffuseColor_id40;
}

vec3 o19S248C0_o18S2C0_o17S2C0_o13S2C3_MaterialSpecularMicrofacetEnvironmentGGXLUT_Compute(vec3 specularColor, float alphaR, float nDotV)
{
    float glossiness = 1.0 - sqrt(alphaR);
    vec4 environmentLightingDFG = textureLod(SPIRV_Cross_CombinedMaterialSpecularMicrofacetEnvironmentGGXLUT_EnvironmentLightingDFG_LUTTexturing_LinearSampler, vec2(glossiness, nDotV), 0.0);
    return (specularColor * environmentLightingDFG.x) + vec3(environmentLightingDFG.y);
}

vec3 o19S248C0_o18S2C0_o17S2C0_MaterialSurfaceShadingSpecularMicrofacet_ComputeEnvironmentLightContribution(PS_STREAMS _streams)
{
    vec3 specularColor = _streams.matSpecularVisible_id28;
    vec3 param = specularColor;
    float param_1 = _streams.alphaRoughness_id27;
    float param_2 = _streams.NdotV_id29;
    return o19S248C0_o18S2C0_o17S2C0_o13S2C3_MaterialSpecularMicrofacetEnvironmentGGXLUT_Compute(param, param_1, param_2) * _streams.envLightSpecularColor_id41;
}

void o19S248C0_o18S2C0_MaterialSurfaceLightingAndShading_Compute(inout PS_STREAMS _streams)
{
    vec3 param = _streams.matNormal_id9;
    NormalUpdate_UpdateNormalFromTangentSpace(_streams, param);
    if (!_streams.IsFrontFace_id1)
    {
        _streams.normalWS_id6 = -_streams.normalWS_id6;
    }
    o19S248C0_o18S2C0_LightStream_ResetLightStream(_streams);
    o19S248C0_o18S2C0_MaterialPixelStream_PrepareMaterialForLightingAndShading(_streams);
    vec3 directLightingContribution = vec3(0.0);
    o0S418C0_DirectLightGroup_PrepareDirectLights();
    int maxLightCount = o0S418C0_LightDirectionalGroup_8__GetMaxLightCount();
    int count = o0S418C0_DirectLightGroupPerView_GetLightCount();
    for (int i = 0; i < maxLightCount; i++)
    {
        if (i >= count)
        {
            break;
        }
        int param_1 = i;
        o0S418C0_DirectLightGroup_PrepareDirectLight(_streams, param_1);
        MaterialPixelShadingStream_PrepareMaterialPerDirectLight(_streams);
        directLightingContribution += o19S248C0_o18S2C0_o12S2C0_MaterialSurfaceShadingDiffuseLambert_true__ComputeDirectLightContribution(_streams);
        directLightingContribution += o19S248C0_o18S2C0_o17S2C0_MaterialSurfaceShadingSpecularMicrofacet_ComputeDirectLightContribution(_streams);
    }
    vec3 environmentLightingContribution = vec3(0.0);
    o1S403C0_LightSimpleAmbient_PrepareEnvironmentLight(_streams);
    environmentLightingContribution += o19S248C0_o18S2C0_o12S2C0_MaterialSurfaceShadingDiffuseLambert_true__ComputeEnvironmentLightContribution(_streams);
    environmentLightingContribution += o19S248C0_o18S2C0_o17S2C0_MaterialSurfaceShadingSpecularMicrofacet_ComputeEnvironmentLightContribution(_streams);
    _streams.shadingColor_id30 += ((directLightingContribution * 3.1415927410125732421875) + environmentLightingContribution);
    _streams.shadingColorAlpha_id31 = _streams.matDiffuse_id11.w;
}

void o19S248C0_MaterialSurfaceArray_Compute(inout PS_STREAMS _streams)
{
    o19S248C0_o3S2C0_MaterialSurfaceDiffuse_Compute(_streams);
    o19S248C0_o5S2C0_MaterialSurfaceNormalMap_false_true__Compute(_streams);
    o19S248C0_o7S2C0_MaterialSurfaceGlossinessMap_false__Compute(_streams);
    o19S248C0_o9S2C0_MaterialSurfaceSetStreamFromComputeColor_matSpecular_rgb__Compute(_streams);
    o19S248C0_o11S2C0_MaterialSurfaceSetStreamFromComputeColor_matSpecularIntensity_r__Compute(_streams);
    o19S248C0_o18S2C0_MaterialSurfaceLightingAndShading_Compute(_streams);
}

vec4 MaterialSurfacePixelStageCompositor_Shading(inout PS_STREAMS _streams)
{
    _streams.viewWS_id25 = normalize(PerView_var.Transformation_Eye.xyz - _streams.PositionWS_id8.xyz);
    _streams.shadingColor_id30 = vec3(0.0);
    o26S248C1_MaterialPixelShadingStream_ResetStream(_streams);
    o19S248C0_MaterialSurfaceArray_Compute(_streams);
    return vec4(_streams.shadingColor_id30, _streams.shadingColorAlpha_id31);
}

void ShadingBase_PSMain(inout PS_STREAMS _streams)
{
    ShaderBase_PSMain();
    vec4 _13 = MaterialSurfacePixelStageCompositor_Shading(_streams);
    _streams.ColorTarget_id2 = _13;
}

void main()
{
    PS_STREAMS _streams = PS_STREAMS(vec4(0.0), false, vec4(0.0), 0.0, vec3(0.0), vec4(0.0), vec3(0.0), mat3(vec3(0.0), vec3(0.0), vec3(0.0)), vec4(0.0), vec3(0.0), vec4(0.0), vec4(0.0), 0.0, vec3(0.0), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, vec4(0.0), 0.0, vec2(0.0), vec3(0.0), 0.0, vec3(0.0), vec3(0.0), 0.0, vec3(0.0), 0.0, vec3(0.0), 0.0, vec3(0.0), 0.0, 0.0, 0.0, vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), 0.0, 0.0, vec3(0.0), vec2(0.0));
    _streams.ShadingPosition_id0 = PS_IN_ShadingPosition;
    _streams.meshNormal_id4 = PS_IN_meshNormal;
    _streams.meshTangent_id5 = PS_IN_meshTangent;
    _streams.PositionWS_id8 = PS_IN_PositionWS;
    _streams.TexCoord_id45 = PS_IN_TexCoord;
    _streams.IsFrontFace_id1 = PS_IN_IsFrontFace;
    NormalFromNormalMapping_GenerateNormal_PS(_streams);
    ShadingBase_PSMain(_streams);
    PS_OUT_ColorTarget = _streams.ColorTarget_id2;
}

