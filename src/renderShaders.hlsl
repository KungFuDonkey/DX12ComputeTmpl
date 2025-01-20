
struct VSData
{
    float4 position : SV_Position;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

VSData vsMain(VSData input)
{
    return input;
}

float4 psMain(PSInput input) : SV_TARGET
{
    return float4(0.0, 1.0, 0.0, 1.0);
}