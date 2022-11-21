enum class DimensionType
{
    WidthHeightEven,
    WidthHeightOdd,
    WidthEvenHeightOdd,
    WidthOddHeightEven,
};

struct GenerateMipMapData
{
    uint sourceMipLevel;
    uint numberOfMipLevels;
    DimensionType dimensionType;
    bool isSrgb;
    float2 texelSize;
};

ConstantBuffer<GenerateMipMapData> mipGenBuffer : register(b0, space2);

Texture2D<float4> sourceMip : register(t0, space2);

RWTexture2D<float4> outputMip1 : register(u0, space2);
RWTexture2D<float4> outputMip2 : register(u1, space2);
RWTexture2D<float4> outputMip3 : register(u2, space2);
RWTexture2D<float4> outputMip4 : register(u3, space2);

SamplerState bilinearSampler : register(s0, space2);

// The color channels are group shared (i.e all threads within a thread group can access them).
// They are separated to reduce bank conflicts.

groupshared float sharedRed[64];
groupshared float sharedGreen[64];
groupshared float sharedBlue[64];
groupshared float sharedAlpha[64];

void storeColor(uint index, float4 color)
{
    sharedRed[index] = color.r;
    sharedGreen[index] = color.g;
    sharedBlue[index] = color.b;
    sharedAlpha[index] = color.a;
}

float4 getColor(uint index) { return float4(sharedRed[index], sharedGreen[index], sharedBlue[index], sharedAlpha[index]); }

float3 srgbToLinear(float3 srgbColor) { return srgbColor < 0.04045f ? srgbColor / 12.92 : pow((srgbColor + 0.055) / 1.055, 2.4); }

float3 linearToSrgb(float3 linearColor) { return linearColor < 0.0031308 ? 12.92 * linearColor : 1.055 * pow(abs(linearColor), 1.0 / 2.4) - 0.055; }

float4 packColor(float4 color)
{
    if (mipGenBuffer.isSRGB)
    {
        return float4(srgbToLinear(color.xyz), color.a);
    }

    return color;
}

[numthreads(8, 8, 1)] void CsMain(uint3 dispatchThreadID
                                  : SV_DispatchThreadID)
{
    float4 sourceColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

    switch (generateMipMapBuffer.dimensionType)
    {
        case DimensionType::WidthHeightEvent:
            {
                // In this case, we perform bilinear filter on four nearest pixels.
                float2 uv = mipGenBuffer.texelSize * (dispatchThreadID.xy + 0.5f);
                sourceColor = sourceMip.sample(bilinearSampler, uv, mipGenBuffer.sourceMipLevel);
            }
            break;

        case DimensionType::WidthHeightOdd:
            {
            }
            break;
    }
}