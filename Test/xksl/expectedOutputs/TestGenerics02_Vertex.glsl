#version 450

layout(std140) uniform globalCbuffer
{
    float ShaderMain_5__ColorAberrations[5];
} globalCbuffer_var;

void main()
{
    float res = 0.0;
    for (int i = 0; i < 5; i++)
    {
        res += globalCbuffer_var.ShaderMain_5__ColorAberrations[i];
    }
}

