struct VertexInput
{
    float3 position : POSITION;
    float2 textureCoord : TEXTURE_COORD;
    float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 textureCoord : TEXTURE_COORD;
};

struct TransformData
{
    row_major matrix modelMatrix;
};

struct SceneData
{
    row_major matrix viewProjectionMatrix;
};

ConstantBuffer<SceneData> sceneBuffer : register(b0);

ConstantBuffer<TransformData> transformBuffer : register(b1);

VertexOutput VsMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0f), mul(transformBuffer.modelMatrix, sceneBuffer.viewProjectionMatrix));
    output.textureCoord = input.textureCoord;

    return output;
}

SamplerState textureSampler : register(s0, space1);
Texture2D<float4> albedoTexture : register(t0, space1);

float4 PsMain(VertexOutput input) : SV_Target
{
    float3 abledoColor = albedoTexture.Sample(textureSampler, input.textureCoord).xyz;
    return float4(abledoColor, 1.0f);
}