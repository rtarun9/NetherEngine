struct VsInput
{
    float3 position : POSITION;
    float2 textureCoords : TEXTURE_COORDS;
    float3 normal : NORMAL;
};

struct VsOutput
{
    float4 position : SV_Position;
    float2 textureCoords : TEXTURE_COORDS;
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
    output.textureCoords = input.textureCoords;

    return output;
}

SamplerState samplerState : register(s0, space1);
Texture2D<float4> albedoTexture : register(t0, space1);

float4 PsMain(VsOutput input) : SV_Target
{
    float3 color = albedoTexture.Sample(samplerState, input.textureCoords).xyz;
    float3 gammaCorrectedColor = pow(color, 1/2.2f);
    return float4(gammaCorrectedColor, 1.0f);
}
