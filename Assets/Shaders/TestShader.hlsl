#include "StaticSamplers.hlsli"

struct VsInput
{
    float3 position : POSITION;
    float2 textureCoords : TEXTURE_COORDS;
    float3 normal : NORMAL;
};

struct VsOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 textureCoords : TEXTURE_COORDS;
};

struct MVPBuffer
{
    row_major matrix modelMatrix;
    matrix inverseModelMatrix;
    row_major matrix viewProjectionMatrix;
};

ConstantBuffer<MVPBuffer> mvpBuffer : register(b0, space0);

VsOutput VsMain(VsInput input)
{
    VsOutput output;
    output.position = mul(float4(input.position, 1.0f), mul(mvpBuffer.modelMatrix, mvpBuffer.viewProjectionMatrix));
    output.textureCoords = input.textureCoords;
    output.normal = mul(input.normal, (float3x3)mvpBuffer.inverseModelMatrix);

    return output;
}

struct SceneConstantBufferData
{
    // Light is initially looking down (i.e) from the light source.
    float3 directionalLightDirection;
    float padding1;
    float3 directionalLightColor;
    float padding2;
    float3 cameraPosition;
    float padding3;
};


Texture2D<float4> albedoTexture : register(t0, space1);
ConstantBuffer<SceneConstantBufferData> sceneBuffer : register(b0, space1);

float4 PsMain(VsOutput input) : SV_Target
{

    float3 color = albedoTexture.Sample(anisotropicSampler, input.textureCoords).xyz;

    // Ambient color term.
    float3 ambientColor = color * 0.1f;
    
    // Diffuse color term.
    float3 pixelToLightDirection = normalize(-sceneBuffer.directionalLightDirection);
    float cosTheta = saturate(dot(pixelToLightDirection, input.normal));
    float3 diffuseColor = color * cosTheta * sceneBuffer.directionalLightColor;

    float3 result = ambientColor + diffuseColor;

    float3 gammaCorrectedColor = pow(result, 1 / 2.2f);
    return float4(gammaCorrectedColor, 1.0f);
}
