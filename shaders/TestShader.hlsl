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

SamplerState textureSampler : register(s0, space1);

float4 PsMain(VertexOutput input) : SV_Target
{
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[renderResources.albedoTextureIndex];

    float3 abledoColor = albedoTexture.Sample(textureSampler, input.textureCoord).xyz;
    return float4(abledoColor, 1.0f);
}
