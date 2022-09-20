struct VsInput
{
    float3 position : POSITION;
    float2 textureCoords : TEXTURE_COORDS;
    float3 normal : NORMAL;
};

struct VsOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

struct MVPBuffer
{
    row_major matrix modelMatrix;
    row_major matrix viewProjectionMatrix;
};

ConstantBuffer<MVPBuffer> mvpBuffer : register(b0, space0);

VsOutput VsMain(VsInput input)
{
    VsOutput output;
    output.position = mul(float4(input.position, 1.0f), mul(mvpBuffer.modelMatrix, mvpBuffer.viewProjectionMatrix));
    output.color = 2.0f * input.position - 1.0f;

    return output;
}

float4 PsMain(VsOutput input) : SV_Target
{
    float3 gammaCorrectedColor = pow(input.color, 1/2.2f);
    return float4(gammaCorrectedColor, 1.0f);
}
