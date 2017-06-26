struct PS_STREAMS
{
    int sbase1_id0;
};

cbuffer Globals
{
    int Base_Var1;
};

static int PS_IN_sbase1;

struct SPIRV_Cross_Input
{
    int PS_IN_sbase1 : TEXCOORD0;
};

int Base_ComputeBase(PS_STREAMS _streams)
{
    return Base_Var1 + _streams.sbase1_id0;
}

int shaderA_f1(PS_STREAMS _streams)
{
    return Base_ComputeBase(_streams);
}

void frag_main()
{
    PS_STREAMS _streams = { 0 };
    _streams.sbase1_id0 = PS_IN_sbase1;
    int i = ((Base_ComputeBase(_streams) + Base_Var1) + Base_ComputeBase(_streams)) + shaderA_f1(_streams);
}

void main(SPIRV_Cross_Input stage_input)
{
    PS_IN_sbase1 = stage_input.PS_IN_sbase1;
    frag_main();
}
