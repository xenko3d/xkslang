#version 450 core

layout(triangles) in;

in gl_PerVertex {
    float gl_PointSize;
} gl_in[];

out gl_PerVertex {
    float gl_PointSize;
};

layout(line_strip) out;
layout(max_vertices = 127) out;
layout(invocations = 4) in;

void main()
{
    float p = gl_in[1].gl_PointSize;
}
