#ifndef COMMON_HLSLI
#define COMMON_HLSLI

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

    float3 directionalLightColor;
    float padding3;

    float3 viewSpaceDirectionalLightPosition;
    float padding4;
};

#endif