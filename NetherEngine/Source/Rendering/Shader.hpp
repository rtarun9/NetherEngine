#pragma once

#include "Resources.hpp"

namespace nether::rendering
{
	// For now can be used as an out parameter in the compile shader function. 
	struct ShaderReflection
	{
		std::vector<InputElementDesc> inputElementDescs{};
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters{};
		std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges{};

		// The name of texture / root cbuffer etc is associated with the root parameter index for ease of use when binding various resources to the pipeline.
		std::unordered_map<std::wstring, uint32_t> rootParameterBindingMap{};
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

		// Base shader directory is a (temporary) fix so that that we can include .hlsli file to shaders, and will almost always be AssetDirectory (from Engine class) / Shaders.
		Shader CompilerShader(const ShaderType& shaderType, const std::wstring_view shaderPath, const std::wstring_view baseShaderDirectory);
		
		// Fill the shader reflection stuff passed into the object passed into the function, and output just the shader blob.
		Microsoft::WRL::ComPtr<IDxcBlob> CompilerShader(const ShaderType& shaderType, const std::wstring_view shaderPath, const std::wstring_view baseShaderDirectory, ShaderReflection& shaderReflection);

	private:
		ShaderCompiler() = default;

	private:
		Microsoft::WRL::ComPtr<IDxcUtils> mUtils{};
		Microsoft::WRL::ComPtr<IDxcCompiler3> mCompiler{};
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> mIncludeHandler{};
	};
}