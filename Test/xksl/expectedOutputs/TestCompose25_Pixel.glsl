#version 450

int o0S12C0_TestDirectLightGroup_GetMaxLightCount()
{
    return 1;
}

int o1S12C0_TestDirectLightGroup_GetMaxLightCount()
{
    return 1;
}

void o2S2C0_TestMaterialSurfaceArray_Compute()
{
    int res = 0;
    res += o0S12C0_TestDirectLightGroup_GetMaxLightCount();
    res += o1S12C0_TestDirectLightGroup_GetMaxLightCount();
}

void main()
{
    o2S2C0_TestMaterialSurfaceArray_Compute();
}

