#include "StaticSamplers.hlsli"

struct VertexOutput
{
    float4 position : SV_Position;
    float2 textureCoord : TEXTURE_COORD;
    float3 normal : NORMAL;
    float3 viewSpaceCoord : WORLD_SPACE_COORD;
};

struct TransformData
{
    row_major matrix modelMatrix;
    row_major matrix inverseModelViewMatrix;
};

struct SceneData
{
    row_major matrix viewMatrix;
    row_major matrix viewProjectionMatrix;
    float3 lightColor;
    float padding;
    float3 viewSpaceLightPosition;
    float padding2;
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
    output.normal = mul(normalBuffer[vertexID], (float3x3)transpose(transformBuffer.inverseModelViewMatrix));
    output.viewSpaceCoord = mul(float4(positionBuffer[vertexID], 1.0f), mul(transformBuffer.modelMatrix, sceneBuffer.viewMatrix)).xyz;

    return output;
}

float4 PsMain(VertexOutput input) : SV_Target
{
    ConstantBuffer<SceneData> sceneBuffer = ResourceDescriptorHeap[renderResources.sceneBufferIndex];
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[renderResources.albedoTextureIndex];

    const float3 albedoColor = albedoTexture.Sample(anisotropicSampler, input.textureCoord).xyz;
    const float ambientStrength = 0.2f;

    const float3 ambientColor = ambientStrength * sceneBuffer.lightColor;

    const float3 normal = normalize(input.normal);
    const float3 pixelToLightDirection = normalize(sceneBuffer.viewSpaceLightPosition - input.viewSpaceCoord);
    const float diffuseStrength = max(dot(pixelToLightDirection, normal), 0.0f);
    
    const float3 diffuseColor = diffuseStrength * sceneBuffer.lightColor;

    const float specularStrength = 0.2f;
    const float3 perfectReflectionDirection = reflect(-pixelToLightDirection, normal);
    const float3 viewDirection = normalize(-input.viewSpaceCoord);
    const float specularIntensity = pow(max(dot(perfectReflectionDirection, viewDirection), 0.0f), 32);
    const float3 specularColor = specularIntensity * sceneBuffer.lightColor;
    
    const float3 color = (ambientColor + diffuseColor + specularColor) * albedoColor;

    return float4(color, 1.0f);
}
