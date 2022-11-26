#ifndef STATIC_SAMPLERS_HLSLI
#define STATIC_SAMPLERS_HLSLI

// To be used in all types of shaders (global).
SamplerState pointClampSampler : register(s0);
SamplerState pointWrapSampler : register(s1);
SamplerState linearClampSampler: register(s2);
SamplerState linearWrapSampler : register(s3);
SamplerState anisotropicSampler : register(s4);

#endif
