#include "StaticSamplers.hlsli"
#include "Common.hlsli"

struct VertexOutput
{
    float4 position : SV_Position;
    float2 textureCoord : TEXTURE_COORD;
    float3 normal : NORMAL;
    float3 viewSpacePosition : WORLD_SPACE_COORD;
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
    output.viewSpacePosition = mul(float4(positionBuffer[vertexID], 1.0f), mul(transformBuffer.modelMatrix, sceneBuffer.viewMatrix)).xyz;

    return output;
}

float4 PsMain(VertexOutput input) : SV_Target
{
    // All lighting is performed in view space, as the viewDirection is very simple to compute (-1 * viewSpacePosition).
    ConstantBuffer<SceneData> sceneBuffer = ResourceDescriptorHeap[renderResources.sceneBufferIndex];
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[renderResources.albedoTextureIndex];

    const float3 albedoColor = albedoTexture.Sample(anisotropicSampler, input.textureCoord).xyz;
    
    // Calculate lighting from point light.
    
    // Calculate the ambient lighting (constant light to simulate continuous bounce of light from all directions).
    // Without it, some parts of the object (away from the light source, usually) will appear completely black.
    const float ambientStrength = 0.2f;
    const float3 ambientColor = ambientStrength * sceneBuffer.lightColor;

    const float3 normal = normalize(input.normal);
    const float3 pixelToLightDirection = normalize(sceneBuffer.viewSpaceLightPosition - input.viewSpacePosition);
    
    // Calculate the diffuse lighting (the directional impact the light has on the pixel).
    const float diffuseStrength = max(dot(pixelToLightDirection, normal), 0.0f);
    const float3 diffuseColor = diffuseStrength * sceneBuffer.lightColor;

    // Calculate the specular lighting (shiny - bright spot that occurs on shiny objects when we are looking near
    // the perfect reflection direction).
    const float specularStrength = 0.5f;
    const float3 perfectReflectionDirection = reflect(-pixelToLightDirection, normal);
    const float3 viewDirection = normalize(-input.viewSpacePosition);

    const float specularIntensity = pow(max(dot(perfectReflectionDirection, viewDirection), 0.0f), 32);
    const float3 specularColor = specularIntensity * sceneBuffer.lightColor * specularStrength;
    
    // Calculate directional light effect (only diffuse for now).
    const float3 directionalLightDirection = normalize(-sceneBuffer.viewSpaceDirectionalLightPosition);

    // Calculate the diffuse lighting (the directional impact the light has on the pixel).
    const float directionalDiffuseStrength = max(dot(directionalLightDirection, normal), 0.0f);
    const float3 directionalDiffuseColor = directionalDiffuseStrength * sceneBuffer.directionalLightColor;

    const float3 color = (ambientColor + diffuseColor + directionalDiffuseColor  + specularColor) * albedoColor;

    return float4(color, 1.0f);
}
