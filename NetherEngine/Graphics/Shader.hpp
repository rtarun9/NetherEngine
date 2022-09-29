#pragma once

namespace nether::graphics
{
	enum class ShaderType
	{
		Vertex,
		Pixel,
	};

	// Holds vector of root parameters, that can be used to create the root signature. Shader object will not hold this as root parameters are to be specified
	// for several shaders in the pipeline.
	// To prevent having to manually enter the root parameter index, use the rootParameterMap which maps the bound resource name to the root parameter.
	struct ShaderReflection
	{
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters{};
		std::unordered_map<std::wstring, uint32_t> rootParameterMap{};
	};

	// Holds shader bytecode for a shader (the shader must contain  the correct entry point (name as VsMain, PsMain, CsMain etc).
	struct Shader
	{
		Microsoft::WRL::ComPtr<IDxcBlob> mShaderBlob{};
	};

	// Main source for learning / using the DXC C++ API : https://laptrinhx.com/using-the-directxshadercompiler-c-api-revised-1531596888/.
	class ShaderCompiler
	{
	public:
		static ShaderCompiler& GetInstance();

		// Returns a Shader (which holds a IDXCBlob), while the shader reflection object is a out parameter.
		Shader CompilerShader(const ShaderType& shaderType, ShaderReflection& shaderReflection, const std::wstring_view shaderPath);
	
	private:
		ShaderCompiler() = default;
		~ShaderCompiler() = default;

		ShaderCompiler(const ShaderCompiler& other) = delete;
		ShaderCompiler& operator=(const ShaderCompiler& other) = delete;
		
		ShaderCompiler(ShaderCompiler&& other) = delete;
		ShaderCompiler& operator=(ShaderCompiler&& other) = delete;
	
	private:
		Microsoft::WRL::ComPtr<IDxcUtils> mUtils{};
		Microsoft::WRL::ComPtr<IDxcCompiler3> mCompiler{};
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> mIncludeHandler{};

		std::wstring mShaderBaseDirectory{};
	};
}

