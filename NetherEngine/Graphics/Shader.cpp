#include "Pch.hpp"

#include "Shader.hpp"

namespace nether::graphics
{
	ShaderCompiler& ShaderCompiler::GetInstance()
	{
		static ShaderCompiler shaderCompiler{};
		return shaderCompiler;
	}

	Shader ShaderCompiler::CompilerShader(const ShaderType& shaderType, ShaderReflection& outShaderReflection, const std::wstring_view shaderPath)
	{
		// Create utils, compiler and include handler if they are not yet created.
		// If mUtils is nullptr, it means the other two members are also null.
		// Also, setup the shader base directory path (derived from shader path).
		if (!mUtils)
		{
			ThrowIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils)));
			ThrowIfFailed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler)));
			ThrowIfFailed(mUtils->CreateDefaultIncludeHandler(&mIncludeHandler));

			std::filesystem::path currentDirectory = std::filesystem::current_path();

			while (!std::filesystem::exists(currentDirectory / "Assets"))
			{
				if (currentDirectory.has_parent_path())
				{
					currentDirectory = currentDirectory.parent_path();
				}
				else
				{
					ErrorMessage(L"Shaders Directory not found!");
				}
			}

			const std::filesystem::path assetsDirectory = currentDirectory / "Assets/Shaders/";

			if (!std::filesystem::is_directory(assetsDirectory))
			{
				ErrorMessage(L"Shaders Directory that was located is not a directory!");
			}

			mShaderBaseDirectory = currentDirectory.wstring() + L"/Assets/Shaders/";

			DebugLogMessage(L"Shader base directory : " + mShaderBaseDirectory);
		}

		// Load the shader source file to a blob.
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob{};
		ThrowIfFailed(mUtils->LoadFile(shaderPath.data(), nullptr, &sourceBlob));

		std::wstring entryPoint{};
		std::wstring targetProfile{};

		switch (shaderType)
		{
		case ShaderType::Vertex:
		{
			entryPoint = L"VsMain";
			targetProfile = L"vs_6_6";
		}break;

		case ShaderType::Pixel:
		{
			entryPoint = L"PsMain";
			targetProfile = L"ps_6_6";
		}break;
		}

		std::vector<LPCWSTR> compilationArguments
		{
			L"-E",
			entryPoint.data(),
			L"-T",
			targetProfile.data(),
			L"-I", mShaderBaseDirectory.data(),
			DXC_ARG_PACK_MATRIX_ROW_MAJOR,
			DXC_ARG_WARNINGS_ARE_ERRORS
		};

		// Indicate that the shader should be in a debuggable state if in debug mode.
		// Else, set optimization level to 03.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			compilationArguments.push_back(DXC_ARG_DEBUG);
		}
		else
		{
			compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
		}

		const DxcBuffer sourceBuffer
		{
			.Ptr = sourceBlob->GetBufferPointer(),
			.Size = sourceBlob->GetBufferSize(),
			.Encoding = 0u
		};

		// Compile the shader.
		Microsoft::WRL::ComPtr<IDxcResult> compiledShaderBuffer{};
		HRESULT hr = mCompiler->Compile(&sourceBuffer, compilationArguments.data(), static_cast<uint32_t>(compilationArguments.size()), mIncludeHandler.Get(), IID_PPV_ARGS(&compiledShaderBuffer));
		if (FAILED(hr))
		{
			ErrorMessage(std::wstring(L"Failed to compile shader with path : ") + shaderPath.data());
		}

		// Get compilation errors (if any).
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
		ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
		if (errors && errors->GetStringLength() > 0)
		{
			const LPCSTR errorMessage = errors->GetStringPointer();
			ErrorMessage(StringToWString(errorMessage));
		}

		// Get shader reflection data.
		Microsoft::WRL::ComPtr<IDxcBlob> reflectionData{};
		ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionData), nullptr));

		const DxcBuffer reflectionBuffer
		{
			.Ptr = reflectionData->GetBufferPointer(),
			.Size = reflectionData->GetBufferSize(),
			.Encoding = 0
		};

		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shaderReflection{};
		mUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
		D3D12_SHADER_DESC shaderDesc{};
		shaderReflection->GetDesc(&shaderDesc);

		// Get root parameters from shader reflection data (constant buffers views will be exclusively used for now).
		outShaderReflection.rootParameters.reserve(shaderDesc.BoundResources);

		// The root parameter at register c0 is the 'RenderResources' struct (holds indices for all non - CBV resources).
		// All CBV's need to be bound based on slot based binding.
		uint32_t srvDescriptorCount{};
		uint32_t registerSpace{};
		std::wstring srvShaderResourceBindingDescName{};

		for (uint32_t i : std::views::iota(0u, shaderDesc.BoundResources))
		{
			D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
			ThrowIfFailed(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));

			outShaderReflection.rootParameterMap[StringToWString(shaderInputBindDesc.Name)] = i;

			ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

			if (shaderInputBindDesc.BindPoint == 0u)
			{
				const D3D12_ROOT_PARAMETER1 rootParameter
				{
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
					.Constants
					{
						.ShaderRegister = shaderInputBindDesc.BindPoint,
						.RegisterSpace = shaderInputBindDesc.Space,
						.Num32BitValues = constantBufferDesc.Size / 8
					}
				};
			
				outShaderReflection.rootParameters.emplace_back(rootParameter);
			}
			else if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER && shaderInputBindDesc.BindPoint != 0)
			{
				const D3D12_ROOT_PARAMETER1 rootParameter
				{
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
					.Descriptor
					{
						.ShaderRegister = shaderInputBindDesc.BindPoint,
						.RegisterSpace = shaderInputBindDesc.Space,
						.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC
					}
				};


				outShaderReflection.rootParameters.emplace_back(rootParameter);
			}
		}

		// Get the shader bytecode (compiled shader output) and return.
		IDxcBlob* compiledShaderBlob{ nullptr };
		compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);

		return Shader
		{
			.mShaderBlob = compiledShaderBlob
		};
	}
}
