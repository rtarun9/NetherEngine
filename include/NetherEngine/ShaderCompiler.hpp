#pragma once

#include "Types.hpp"

namespace nether
{
    enum class ShaderTypes : uint8_t
    {
        Vertex,
        Pixel,
        Compute,
    };
}

// Rather than using a static class, a namespace is used here. The corresponding .cpp file will hold the 'member functions' of the namespace.
namespace nether::ShaderCompiler
{
    // Takes a Pipeline object as inout parameter to setup shader reflection data.
    Shader compile(const ShaderTypes& shaderType, const std::wstring_view shaderPath, GraphicsPipelineReflectionData& pipelineReflectionData);

    Shader compile(const ShaderTypes& shaderType, const std::wstring_view shaderPath);
}
