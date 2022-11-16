#pragma once

namespace nether
{
    enum class ShaderTypes : uint8_t
    {
        Vertex,
        Pixel,
    };
}

// Rather than using a static class, a namespace is used here. The corresponding .cpp file will hold the 'member functions' of the namespace.
namespace nether::ShaderCompiler
{
    Shader compile(const ShaderTypes& shaderType, const std::wstring_view shaderPath);
}
