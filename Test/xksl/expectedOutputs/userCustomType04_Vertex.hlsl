struct ShaderBase_CustomStruct
{
    float3 DirectionWS;
};

struct ShaderMain_CustomStruct
{
    float3 DirectionWS;
    float3 Color;
};

ShaderMain_CustomStruct ShaderMain_GetMainStruct()
{
    ShaderMain_CustomStruct c1;
    return c1;
}

ShaderBase_CustomStruct ShaderMain_GetBaseStruct()
{
    ShaderBase_CustomStruct c1;
    return c1;
}

void ShaderMain_aFunctionTakingMainStruct(ShaderMain_CustomStruct c)
{
}

void ShaderMain_aFunctionTakingBaseStruct(ShaderBase_CustomStruct c)
{
}

ShaderBase_CustomStruct ShaderMain_ConvertStreamsToShaderBaseStreams(ShaderMain_CustomStruct c)
{
    ShaderBase_CustomStruct c1 = { c.DirectionWS };
    return c1;
}

void vert_main()
{
    ShaderMain_CustomStruct c1 = ShaderMain_GetMainStruct();
    ShaderBase_CustomStruct c2 = ShaderMain_GetBaseStruct();
    ShaderMain_CustomStruct param = c1;
    ShaderMain_aFunctionTakingMainStruct(param);
    ShaderBase_CustomStruct param_1 = c2;
    ShaderMain_aFunctionTakingBaseStruct(param_1);
    ShaderMain_CustomStruct param_2 = c1;
    c2 = ShaderMain_ConvertStreamsToShaderBaseStreams(param_2);
}

void main()
{
    vert_main();
}
