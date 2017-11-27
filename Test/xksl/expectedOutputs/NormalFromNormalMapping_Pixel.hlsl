struct PS_STREAMS
{
    float3 meshNormal_id0;
    float4 meshTangent_id1;
    float3x3 tangentToWorld_id2;
    float4 ShadingPosition_id3;
};

cbuffer PerDraw
{
    column_major float4x4 Transformation_WorldInverse;
    column_major float4x4 Transformation_WorldInverseTranspose;
    column_major float4x4 Transformation_WorldView;
    column_major float4x4 Transformation_WorldViewInverse;
    column_major float4x4 Transformation_WorldViewProjection;
    float3 Transformation_WorldScale;
    float4 Transformation_EyeMS;
};

static float3 PS_IN_meshNormal;
static float4 PS_IN_meshTangent;
static float4 PS_IN_ShadingPosition;

struct SPIRV_Cross_Input
{
    float3 PS_IN_meshNormal : NORMAL;
    float4 PS_IN_meshTangent : TANGENT;
    float4 PS_IN_ShadingPosition : SV_Position;
};

void NormalUpdate_GenerateNormal_PS()
{
}

float3x3 NormalUpdate_GetTangentMatrix(inout PS_STREAMS _streams)
{
    _streams.meshNormal_id0 = normalize(_streams.meshNormal_id0);
    float3 tangent = normalize(_streams.meshTangent_id1.xyz);
    float3 bitangent = cross(_streams.meshNormal_id0, tangent) * _streams.meshTangent_id1.w;
    float3x3 tangentMatrix = float3x3(float3(tangent), float3(bitangent), float3(_streams.meshNormal_id0));
    return tangentMatrix;
}

float3x3 NormalFromNormalMapping_GetTangentWorldTransform()
{
    return float3x3(float3(Transformation_WorldInverseTranspose[0].x, Transformation_WorldInverseTranspose[0].y, Transformation_WorldInverseTranspose[0].z), float3(Transformation_WorldInverseTranspose[1].x, Transformation_WorldInverseTranspose[1].y, Transformation_WorldInverseTranspose[1].z), float3(Transformation_WorldInverseTranspose[2].x, Transformation_WorldInverseTranspose[2].y, Transformation_WorldInverseTranspose[2].z));
}

void NormalUpdate_UpdateTangentToWorld(inout PS_STREAMS _streams)
{
    float3x3 _67 = NormalUpdate_GetTangentMatrix(_streams);
    float3x3 tangentMatrix = _67;
    float3x3 tangentWorldTransform = NormalFromNormalMapping_GetTangentWorldTransform();
    _streams.tangentToWorld_id2 = mul(tangentMatrix, tangentWorldTransform);
}

void NormalFromNormalMapping_GenerateNormal_PS(inout PS_STREAMS _streams)
{
    NormalUpdate_GenerateNormal_PS();
    NormalUpdate_UpdateTangentToWorld(_streams);
}

void ShaderBase_PSMain()
{
}

void frag_main()
{
    PS_STREAMS _streams = { float3(0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float3x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)), float4(0.0f, 0.0f, 0.0f, 0.0f) };
    _streams.meshNormal_id0 = PS_IN_meshNormal;
    _streams.meshTangent_id1 = PS_IN_meshTangent;
    _streams.ShadingPosition_id3 = PS_IN_ShadingPosition;
    NormalFromNormalMapping_GenerateNormal_PS(_streams);
    ShaderBase_PSMain();
}

void main(SPIRV_Cross_Input stage_input)
{
    PS_IN_meshNormal = stage_input.PS_IN_meshNormal;
    PS_IN_meshTangent = stage_input.PS_IN_meshTangent;
    PS_IN_ShadingPosition = stage_input.PS_IN_ShadingPosition;
    frag_main();
}
