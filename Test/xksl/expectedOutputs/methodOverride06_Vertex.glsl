#version 410
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

layout(std140) uniform Globals
{
    float ShaderMainX_varMainX;
    float ShaderMain_varMain;
    float o0S15C0_ShaderCompose_varCompose;
    float o0S15C0_ShaderComposeX2_varComposeX2;
    float o0S15C0_ShaderComposeX1_varComposeX1;
} Globals_var;

float ShaderMain_Compute()
{
    return Globals_var.ShaderMain_varMain;
}

float ShaderMainX_Compute()
{
    return ShaderMain_Compute() + Globals_var.ShaderMainX_varMainX;
}

float o0S15C0_ShaderComposeX1_Compute()
{
    return ShaderMainX_Compute() + Globals_var.o0S15C0_ShaderComposeX1_varComposeX1;
}

float o0S15C0_ShaderComposeX2_Compute()
{
    return o0S15C0_ShaderComposeX1_Compute() + Globals_var.o0S15C0_ShaderComposeX2_varComposeX2;
}

float o0S15C0_ShaderCompose_ComputeComp()
{
    return Globals_var.o0S15C0_ShaderCompose_varCompose + o0S15C0_ShaderComposeX2_Compute();
}

void main()
{
    float f = o0S15C0_ShaderComposeX2_Compute();
    f += o0S15C0_ShaderCompose_ComputeComp();
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
    gl_Position.y = -gl_Position.y;
}

