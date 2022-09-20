#pragma once

#include "Resources.hpp"

namespace nether::rendering
{
	struct ShaderReflection
	{
		std::vector<InputElementDesc> inputElementDescs{};
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters{};
	};

	// Hold the shader blob and shader reflection data.
	struct Shader
	{
		Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob{};
		ShaderReflection shaderReflection;
	};

	enum class ShaderType
	{
		Vertex,
		Pixel
	};

	// Main source for learning / using the DXC C++ API : https://laptrinhx.com/using-the-directxshadercompiler-c-api-revised-1531596888/.
	class ShaderCompiler
	{
	public:
		static ShaderCompiler& GetInstance();

		Shader CompilerShader(const ShaderType& shaderType, const std::wstring_view shaderPath);

	private:
		ShaderCompiler() = default;

	private:
		Microsoft::WRL::ComPtr<IDxcUtils> mUtils{};
		Microsoft::WRL::ComPtr<IDxcCompiler3> mCompiler{};
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> mIncludeHandler{};
	};
}