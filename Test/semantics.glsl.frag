
#version 140

//PIXEL SHADER
void main()
{    
	vec4 fragCoord = gl_FragCoord;
	float frontFacing = gl_FrontFacing? 0:1;
	
	//pixel shader outputs
	gl_FragColor = fragCoord;
	gl_FragDepth = frontFacing;
	
	//gl_FragData[0] = vec4(0.0);  //can use either gl_FragColor or gl_FragData

	//pixel shader input
	//vec4 fragCoord = gl_FragCoord;
	//bool b = gl_FrontFacing;          //Unsupported builtin in HLSL: builtInFrontFacing
	//vec2 pointCoord = gl_PointCoord;  //Unsupported builtin in HLSL: builtInPointCoord
}

//=========================================================================
//Glslang: GLSL --> SPV conversion
//D:\Prgms\glslang\source\Test\semantics.glsl.frag -e main --keep-uncalled -V -H -o D:\Prgms\glslang\source\Test\semantics.glsl.frag.spv

//SPV outputs
/*
// Module Version 10000
// Generated by (magic number): 80001
// Id's are bound by 30

                              Capability Shader
               1:             ExtInstImport  "GLSL.std.450"
                              MemoryModel Logical GLSL450
                              EntryPoint Fragment 4  "main" 11 17 25 28
                              ExecutionMode 4 OriginUpperLeft
                              ExecutionMode 4 DepthReplacing
                              Source GLSL 140
                              Name 4  "main"
                              Name 9  "fragCoord"
                              Name 11  "gl_FragCoord"
                              Name 14  "frontFacing"
                              Name 17  "gl_FrontFacing"
                              Name 25  "gl_FragColor"
                              Name 28  "gl_FragDepth"
                              Decorate 11(gl_FragCoord) BuiltIn FragCoord
                              Decorate 17(gl_FrontFacing) BuiltIn FrontFacing
                              Decorate 28(gl_FragDepth) BuiltIn FragDepth
               2:             TypeVoid
               3:             TypeFunction 2
               6:             TypeFloat 32
               7:             TypeVector 6(float) 4
               8:             TypePointer Function 7(fvec4)
              10:             TypePointer Input 7(fvec4)
11(gl_FragCoord):     10(ptr) Variable Input
              13:             TypePointer Function 6(float)
              15:             TypeBool
              16:             TypePointer Input 15(bool)
17(gl_FrontFacing):     16(ptr) Variable Input
              19:             TypeInt 32 1
              20:     19(int) Constant 0
              21:     19(int) Constant 1
              24:             TypePointer Output 7(fvec4)
25(gl_FragColor):     24(ptr) Variable Output
              27:             TypePointer Output 6(float)
28(gl_FragDepth):     27(ptr) Variable Output
         4(main):           2 Function None 3
               5:             Label
    9(fragCoord):      8(ptr) Variable Function
 14(frontFacing):     13(ptr) Variable Function
              12:    7(fvec4) Load 11(gl_FragCoord)
                              Store 9(fragCoord) 12
              18:    15(bool) Load 17(gl_FrontFacing)
              22:     19(int) Select 18 20 21
              23:    6(float) ConvertSToF 22
                              Store 14(frontFacing) 23
              26:    7(fvec4) Load 9(fragCoord)
                              Store 25(gl_FragColor) 26
              29:    6(float) Load 14(frontFacing)
                              Store 28(gl_FragDepth) 29
                              Return
                              FunctionEnd
*/

//=========================================================================
//SPIRV-Cross: SPV --> GLSL
//--output D:\Prgms\glslang\source\Test\xksl\outputs\semantics.glsl.output.glsl D:\Prgms\glslang\source\Test\semantics.glsl.frag.spv
/*

#version 140
#ifdef GL_ARB_shading_language_420pack
#extension GL_ARB_shading_language_420pack : require
#endif

out vec4 _gl_FragColor;

void main()
{
    vec4 fragCoord = gl_FragCoord;
    _gl_FragColor = fragCoord;
}


*/

//=========================================================================
//SPIRV-Cross: SPV --> HLSL
//--hlsl --output D:\Prgms\glslang\source\Test\xksl\outputs\semantics.glsl.output.hlsl D:\Prgms\glslang\source\Test\semantics.glsl.frag.spv
/*

static float4 gl_FragCoord;
static float4 gl_FragColor;

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : VPOS;
};

struct SPIRV_Cross_Output
{
    float4 gl_FragColor : COLOR0;
};

void frag_main()
{
    float4 fragCoord = gl_FragCoord;
    gl_FragColor = fragCoord;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord + float4(0.5f, 0.5f, 0.0f, 0.0f);
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragColor = gl_FragColor;
    return stage_output;
}


*/