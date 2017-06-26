struct PS_STREAMS
{
    int ColorTarget_id0;
};

static int PS_OUT_ColorTarget;

struct SPIRV_Cross_Output
{
    int PS_OUT_ColorTarget : TOTO;
};

int OverrideB_Compute()
{
    return 5;
}

void frag_main()
{
    PS_STREAMS _streams = { 0 };
    _streams.ColorTarget_id0 = 1 + OverrideB_Compute();
    PS_OUT_ColorTarget = _streams.ColorTarget_id0;
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.PS_OUT_ColorTarget = PS_OUT_ColorTarget;
    return stage_output;
}
