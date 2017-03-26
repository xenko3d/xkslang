// D:\Prgms\glslang\source\Test\hlsl.test.frag --keep-uncalled -D -V -o D:\Prgms\glslang\source\Test\hlsl.test.frag.latest.spv
//D:\Prgms\glslang\source\Test\hlsl.test.frag --keep-uncalled -D -V -H -o D:\Prgms\glslang\source\Test\hlsl.test.fragB.latest.spv.hr

struct PSInput
{
    float  myfloat    : SOME_SEMANTIC;
    int    something  : ANOTHER_SEMANTIC;
};

[maxvertexcount(4)]
void main(triangle in uint VertexID[3] : VertexID,
          triangle uint test[3] : FOO, 
          inout LineStream<PSInput> OutputStream)
{
    PSInput Vert;

    Vert.myfloat    = test[0] + test[1] + test[2];
    Vert.something  = VertexID[0];

    OutputStream.Append(Vert);
    OutputStream.Append(Vert);
    OutputStream.RestartStrip();
}
