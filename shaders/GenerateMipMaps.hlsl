#include "Common.hlsli"
#include "StaticSamplers.hlsli"

// The dimension type will determine how many samples of source texture are required to correctly
// sample the texture (i.e if width or height is odd, to prevent undersampling, multiple textures have to be taken).
enum class DimensionType : uint
{
    WidthHeightEven,    // A single bilinear sample such that 4 source texture pixels are averaged is required.
    WidthHeightOdd,     // 4 bilinear samples are to be taken at different offsets ((0.25, 0.25), (0.5, 0.25), (0.25, 0.5), (0.5, 0.5))
    WidthEvenHeightOdd, // Take 2 bilinear samples at offsets (0.5, 0.25), (0.5, 0.75)
    WidthOddHeightEven  // Take 2 bilinear samples at offsets (0.25, 0.5), (0.75, 0.5)
};

struct GenerateMipMapData
{
    uint sourceMipLevel;
    uint numberOfMipLevels;
    DimensionType dimensionType;
    uint isSRGB;
    float2 texelSize;
};

struct RenderResources
{
    uint mipGenBufferIndex;
    uint sourceTextureIndex;
    uint outputMip1Index;
    uint outputMip2Index;
    uint outputMip3Index;
    uint outputMip4Index;
};

ConstantBuffer<RenderResources> renderResources : register(b0);


// The color channels are group shared (i.e all threads within a thread group can access them).
// They are separated to reduce bank conflicts on LDS (local data store).
// Required as a single dispatch will atmost generate 4 mip levels, due to which downsampling can occur multiple times.
// Using this format will cause only 2 bank conflicts rather than 8, if we had groupshared float4 sharedColor[64].
// Resource on CS + shader memory : https://www.youtube.com/watch?v=eDLilzy2mq0
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

// Before storing color in output texture, convert it to the correct format.
float4 packColor(float4 color, uint isSRGB)
{
    if (isSRGB)
    {
        return float4(linearToSrgb(color.xyz), color.a);
    }

    return color;
}

// Dispatch Thread ID : 3D ID of the thread within the entire dispatch.
// Gropu index is the flattend index of a thread within the thread group.
[numthreads(8, 8, 1)] void CsMain(uint3 dispatchThreadID
                                  : SV_DispatchThreadID, uint groupIndex
                                  : SV_GroupIndex)
{
    ConstantBuffer<GenerateMipMapData> mipGenBuffer = ResourceDescriptorHeap[renderResources.mipGenBufferIndex];
    
    Texture2D<float4> sourceTexture = ResourceDescriptorHeap[renderResources.sourceTextureIndex];
    RWTexture2D<float4> outputMip1 = ResourceDescriptorHeap[renderResources.outputMip1Index];

    float4 sourceColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

    switch (mipGenBuffer.dimensionType)
    {
        case DimensionType::WidthHeightEven:
            {
                float2 uv = mipGenBuffer.texelSize * (dispatchThreadID.xy + 0.5f);
                sourceColor = sourceTexture.SampleLevel(linearClampSampler, uv, mipGenBuffer.sourceMipLevel);
            }
            break;

        case DimensionType::WidthHeightOdd:
            {
                float2 offset = mipGenBuffer.texelSize * 0.5f;
                float2 uv = mipGenBuffer.texelSize * (dispatchThreadID.xy + 0.25f);

                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv, mipGenBuffer.sourceMipLevel);
                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv + float2(offset.x, 0.0f), mipGenBuffer.sourceMipLevel);
                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv + float2(0.0f, offset.y), mipGenBuffer.sourceMipLevel);
                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv + offset, mipGenBuffer.sourceMipLevel);
                sourceColor *= 0.25f;
            }
            break;

        case DimensionType::WidthEvenHeightOdd:
            {
                float2 offset = mipGenBuffer.texelSize * float2(0.0f, 0.5f);
                float2 uv = mipGenBuffer.texelSize * (dispatchThreadID.xy + float2(0.5f, 0.25f));

                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv, mipGenBuffer.sourceMipLevel);
                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv + offset, mipGenBuffer.sourceMipLevel);
                sourceColor *= 0.5f;
            }
            break;

        case DimensionType::WidthOddHeightEven:
            {
                float2 offset = mipGenBuffer.texelSize * float2(0.5f, 0.0f);
                float2 uv = mipGenBuffer.texelSize * (dispatchThreadID.xy + float2(0.25f, 0.5f));

                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv, mipGenBuffer.sourceMipLevel);
                sourceColor += sourceTexture.SampleLevel(linearClampSampler, uv + offset, mipGenBuffer.sourceMipLevel);
                sourceColor *= 0.5f;
            }
    }


    // All threads from the thread group will participate in generation of first mip level (note that number of dispatch groups is half of that of source texture).
    outputMip1[dispatchThreadID.xy] = packColor(sourceColor, mipGenBuffer.isSRGB);

    if (mipGenBuffer.numberOfMipLevels == 1)
    {
        return;
    }


    storeColor(groupIndex, sourceColor);

    // Ensures that all LDS (Local data store) writes have occured across all threads in a thread group.
    GroupMemoryBarrierWithGroupSync();

    RWTexture2D<float4> outputMip2 = ResourceDescriptorHeap[renderResources.outputMip2Index];
    
    // The 2nd mip level (relative to the first mip level, that is) requires  only
    // 1/4 of the threads that participated in generation of 1st mip level.
    // In group index, the lwo three bits represent X and higher 3 bits represent Y. For example, the group size is 8x8. The grid will have
    // on the X and Y axis 000, 001, .... 111.
    // As we need only 1/4th of threads of the thread group to participate for 2nd mip level, we can use only those threads whose index in
    // group is even in both X and Y (note that threads in group are in 8x8 fashion).
    // Example : in the threads of group index 000_000, 001_000, 000_001, 001_001, only the first thread will participate (1 of 4).

    // 0x9 : 001001 in binary
    if ((groupIndex & 0x9) == 0)
    {
        sourceColor += getColor(groupIndex + 0x01);  // color immediatly to the right of the current thread.
        sourceColor += getColor(groupIndex + 0x08);  // color below the current thread.
        sourceColor += getColor(groupIndex + 0x09);  // color to the right of the thread.

        sourceColor *= 0.25f;

        outputMip2[dispatchThreadID.xy / 2] = packColor(sourceColor, mipGenBuffer.isSRGB);
    }

    storeColor(groupIndex, sourceColor);

    if (mipGenBuffer.numberOfMipLevels == 2)
    {
        return;
    }

    GroupMemoryBarrierWithGroupSync();

    RWTexture2D<float4> outputMip3 = ResourceDescriptorHeap[renderResources.outputMip3Index];

    // Only those threads within the thread group whose X and Y values of group index are multiples of 4 are required to participate.
    // This is because for the 3rd mip level, only 1/16th of the threads are requierd to participate.
    // 0x1B : 011011 in binary.
    if ((groupIndex & 0x1B) == 0)
    {
        sourceColor += getColor(groupIndex + 0x02);
        sourceColor += getColor(groupIndex + 0x10);
        sourceColor += getColor(groupIndex + 0x12);

        sourceColor *= 0.25;

        outputMip3[dispatchThreadID.xy / 4] = packColor(sourceColor, mipGenBuffer.isSRGB);
    }

    storeColor(groupIndex, sourceColor);

    if (mipGenBuffer.numberOfMipLevels == 3)
    {
        return;
    }

    GroupMemoryBarrierWithGroupSync();

   RWTexture2D<float4> outputMip4 = ResourceDescriptorHeap[renderResources.outputMip2Index];

   // Only those threads whose X and Y part of index are multiples of 8 are required to participate for generation of 4th mip map.
    // As thread group size is 8x8, only a single thread will satisfy said condition, the thread with group index 000_000.
    if (groupIndex == 0)
    {
        sourceColor += getColor(groupIndex + 0x04);
        sourceColor += getColor(groupIndex + 0x20);
        sourceColor += getColor(groupIndex + 0x24);

        sourceColor *= 0.25f;

        outputMip4[dispatchThreadID.xy / 8] = packColor(sourceColor, mipGenBuffer.isSRGB);
    }

    return;
}