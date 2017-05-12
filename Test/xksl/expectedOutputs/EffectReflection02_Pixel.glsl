#version 450

struct PS_STREAMS
{
    vec4 ColorTarget_id0;
};

layout(std140) uniform PerDraw
{
    layout(row_major) mat2x3 ShaderMain_aMat23;
    mat2x3 ShaderMain_aMat23_rm;
    vec4 ShaderMain_Color;
} PerDraw_var;

layout(location = 0) out vec4 PS_OUT_ColorTarget;

void main()
{
    PS_STREAMS _streams = PS_STREAMS(vec4(0.0));
    _streams.ColorTarget_id0 = PerDraw_var.ShaderMain_Color;
    PS_OUT_ColorTarget = _streams.ColorTarget_id0;
}

