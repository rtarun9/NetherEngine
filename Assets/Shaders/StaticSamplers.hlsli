#ifndef STATIC_SAMPLERS_HLSLI
#define STATIC_SAMPLERS_HLSLI

// Not specifying a state as these samplers are meant to be global (so all shader will use them, even if implicitly they have a space0).
SamplerState anisotropicSampler : register(s0);

#endif