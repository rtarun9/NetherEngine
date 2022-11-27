#include "StaticSamplers.hlsli"

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
    float3 lightColor;
    float padding;
};

struct RenderResources
{
    uint positionBufferIndex;
    uint textureCoordBufferIndex;
    uint normalBufferIndex;

    uint sceneBufferIndex;
    uint transformBufferIndex;

    uint albedoTextureIndex;
};

ConstantBuffer<RenderResources> renderResources : register(b0);

VertexOutput VsMain(uint vertexID : SV_VertexID)
{
    VertexOutput output;

    StructuredBuffer<float3> positionBuffer = ResourceDescriptorHeap[renderResources.positionBufferIndex];
    StructuredBuffer<float2> textureCoordBuffer = ResourceDescriptorHeap[renderResources.textureCoordBufferIndex];
    StructuredBuffer<float3> normalBuffer = ResourceDescriptorHeap[renderResources.normalBufferIndex];

    ConstantBuffer<SceneData> sceneBuffer = ResourceDescriptorHeap[renderResources.sceneBufferIndex];
    ConstantBuffer<TransformData> transformBuffer = ResourceDescriptorHeap[renderResources.transformBufferIndex];

    output.position = mul(float4(positionBuffer[vertexID], 1.0f), mul(transformBuffer.modelMatrix, sceneBuffer.viewProjectionMatrix));
    output.textureCoord = textureCoordBuffer[vertexID];

    return output;
}

float4 PsMain(VertexOutput input) : SV_Target
{
    ConstantBuffer<SceneData> sceneBuffer = ResourceDescriptorHeap[renderResources.sceneBufferIndex];
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[renderResources.albedoTextureIndex];

    float3 albedoColor = albedoTexture.Sample(anisotropicSampler, input.textureCoord).xyz;
    float3 color = albedoColor * sceneBuffer.lightColor;

    return float4(color, 1.0f);
}
