#include "Pch.hpp"

#include "Shader.hpp"

#include "GraphicsCommon.hpp"
#include "Common/Utils.hpp"

namespace nether::rendering
{
	ShaderCompiler& ShaderCompiler::GetInstance()
	{
		static ShaderCompiler shaderCompilerInstance{};
		return shaderCompilerInstance;
	}
	
	Shader ShaderCompiler::CompilerShader(const ShaderType& shaderType, const std::wstring_view shaderPath)
	{
		// Create utils, compiler and include handler if they are not yet created.
		// If mUtils is nullptr, it means the other two members are also null.
		if (!mUtils)
		{
			utils::ThrowIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils)));
			utils::ThrowIfFailed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler)));
			utils::ThrowIfFailed(mUtils->CreateDefaultIncludeHandler(&mIncludeHandler));
		}

		// Load the shader source file to a blob.
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob{};
		utils::ThrowIfFailed(mUtils->LoadFile(shaderPath.data(), nullptr, &sourceBlob));

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
			L"-Qstrip_reflect",
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
			utils::ErrorMessage(std::wstring(L"Failed to compile shader with path : ") + shaderPath.data());
		}

		// Get compilation errors (if any).
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
		utils::ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
		if (errors && errors->GetStringLength() > 0)
		{
			const LPCSTR errorMessage = errors->GetStringPointer();
			utils::ErrorMessage(utils::StringToWString(errorMessage));
		}

		// Get shader reflection data.
		Microsoft::WRL::ComPtr<IDxcBlob> reflectionData{};
		utils::ThrowIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionData), nullptr));

		const DxcBuffer reflectionBuffer
		{
			.Ptr = reflectionData->GetBufferPointer(),
			.Size = reflectionData->GetBufferSize(),
			.Encoding = 0
		};

		Shader shader{};

		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shaderReflection{};
		mUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
		D3D12_SHADER_DESC shaderDesc{};
		shaderReflection->GetDesc(&shaderDesc);

		// Get the input element desc's from the shader reflection desc.
		shader.shaderReflection.inputElementDescs.reserve(shaderDesc.InputParameters);

		for (uint32_t i : std::views::iota(0u, shaderDesc.InputParameters))
		{
			D3D12_SIGNATURE_PARAMETER_DESC inputParameterDesc{};
			utils::ThrowIfFailed(shaderReflection->GetInputParameterDesc(i, &inputParameterDesc));

			DXGI_FORMAT inputElementFormat{ DXGI_FORMAT_UNKNOWN };
			// 15 is 1111 in binary, so a 4 component element.
			// 7 is 111 in binary, so a 3 component element.
			// 3 is 11 in binary, so a 2 component element.
			// 1 is 1 in binary, so a 1 component element.
			switch (inputParameterDesc.ComponentType)
			{
			case D3D_REGISTER_COMPONENT_FLOAT32:
			{
				if (inputParameterDesc.Mask == 15)
				{
					inputElementFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
				if (inputParameterDesc.Mask == 7)
				{
					inputElementFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				}
				else if (inputParameterDesc.Mask == 3)
				{
					inputElementFormat = DXGI_FORMAT_R32G32_FLOAT;
				}
				else if (inputParameterDesc.Mask == 1)
				{
					inputElementFormat = DXGI_FORMAT_R32_FLOAT;
				}
			}break;

			case D3D_REGISTER_COMPONENT_UINT32:
			{
				if (inputParameterDesc.Mask == 15)
				{
					inputElementFormat = DXGI_FORMAT_R32G32B32A32_UINT;
				}
				if (inputParameterDesc.Mask == 7)
				{
					inputElementFormat = DXGI_FORMAT_R32G32B32_UINT;
				}
				else if (inputParameterDesc.Mask == 3)
				{
					inputElementFormat = DXGI_FORMAT_R32G32_UINT;
				}
				else if (inputParameterDesc.Mask == 1)
				{
					inputElementFormat = DXGI_FORMAT_R32_UINT;
				}
			}break;

			case D3D_REGISTER_COMPONENT_SINT32:
			{
				if (inputParameterDesc.Mask == 15)
				{
					inputElementFormat = DXGI_FORMAT_R32G32B32A32_SINT;
				}
				if (inputParameterDesc.Mask == 7)
				{
					inputElementFormat = DXGI_FORMAT_R32G32B32_SINT;
				}
				else if (inputParameterDesc.Mask == 3)
				{
					inputElementFormat = DXGI_FORMAT_R32G32_SINT;
				}
				else if (inputParameterDesc.Mask == 1)
				{
					inputElementFormat = DXGI_FORMAT_R32_SINT;
				}
			}break;
			}

			const D3D12_INPUT_ELEMENT_DESC inputElementDesc
			{
				.SemanticName = inputParameterDesc.SemanticName,
				.SemanticIndex = inputParameterDesc.SemanticIndex,
				.Format = inputElementFormat,
				.InputSlot = 0u,
				.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			};

			shader.shaderReflection.inputElementDescs.emplace_back(InputElementDesc{ .semanticName = inputParameterDesc.SemanticName, .inputElementDesc = inputElementDesc });
		}

		// Get root parameters from shader reflection data (constant buffers views will be exclusively used for now).
		shader.shaderReflection.rootParameters.reserve(shaderDesc.BoundResources);

		// Each shader will have all resources in specific spaces (0 for vertex shader, 1 for pixel shaders).
		// SRV's will all be in the same space (as an assumption) and in a descriptor table.
		// For this, the loop will only count number of descriptor's, and after the loop the descriptor range (and the table) will be filled.
		uint32_t srvDescriptorCount{};
		uint32_t registerSpace{};

		for (uint32_t i : std::views::iota(0u, shaderDesc.BoundResources))
		{
			D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
			utils::ThrowIfFailed(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));

			ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

			switch (shaderInputBindDesc.Type)
			{
			case D3D_SIT_CBUFFER:
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

				shader.shaderReflection.rootParameters.emplace_back(rootParameter);
			}break;

			case D3D_SIT_TEXTURE:
			{
				srvDescriptorCount++;
				registerSpace = shaderInputBindDesc.Space;
			}break;
			}
		}

		if (srvDescriptorCount > 0)
		{
			// Fill the descriptor range.
			const D3D12_DESCRIPTOR_RANGE1 srvDescriptorRange
			{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = srvDescriptorCount,
				.RegisterSpace = registerSpace,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC,
				.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
			};

			shader.shaderReflection.descriptorRanges.push_back(srvDescriptorRange);

			D3D12_SHADER_VISIBILITY shaderVisibility{};
			switch (shaderType)
			{
			case ShaderType::Vertex:
			{
				shaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			}break;

			case ShaderType::Pixel:
			{
				shaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			}break;
			}

			// Create the descriptor table.
			const D3D12_ROOT_PARAMETER1 srvDescriptorTable
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable
				{
					.NumDescriptorRanges = static_cast<uint32_t>(shader.shaderReflection.descriptorRanges.size()),
					.pDescriptorRanges = shader.shaderReflection.descriptorRanges.data(),
				},
				.ShaderVisibility = shaderVisibility,
			};

			shader.shaderReflection.rootParameters.push_back(srvDescriptorTable);
		}
	
		// Get the shader bytecode (compiled shader output) and return.
		IDxcBlob* compiledShaderBlob{ nullptr };
		compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);

		shader.shaderBlob = compiledShaderBlob;

		return shader;
	}
}