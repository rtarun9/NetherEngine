#include "Common.hlsli"
#include "StaticSamplers.hlsli"

struct VertexOutput
{
    float4 position : SV_Position;
    float2 textureCoord : TEXTURE_COORD;
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

    ConstantBuffer<SceneData> sceneBuffer = ResourceDescriptorHeap[renderResources.sceneBufferIndex];
    ConstantBuffer<TransformData> transformBuffer = ResourceDescriptorHeap[renderResources.transformBufferIndex];

    output.position = mul(float4(positionBuffer[vertexID], 1.0f), mul(transformBuffer.modelMatrix, sceneBuffer.viewProjectionMatrix));

    return output;
}

float4 PsMain(VertexOutput input) : SV_Target
{
    ConstantBuffer<SceneData> sceneBuffer = ResourceDescriptorHeap[renderResources.sceneBufferIndex];

    return float4(sceneBuffer.lightColor, 1.0f);
}
