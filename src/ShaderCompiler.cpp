#include "Pch.hpp"

#include "ShaderCompiler.hpp"

namespace nether::ShaderCompiler
{
    // Responsible for the actual compilation of shaders.
    Comptr<IDxcCompiler3> compiler{};

    // Used to create include handle and provides interfaces for loading shader to blob, etc.
    Comptr<IDxcUtils> utils{};
    Comptr<IDxcIncludeHandler> includeHandler{};

    Shader compile(const ShaderTypes& shaderType, const std::wstring_view shaderPath, GraphicsPipelineReflectionData& pipelineReflectionData)
    {
        Shader shader{};

        if (!utils)
        {
            throwIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
            throwIfFailed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
            throwIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));
        }

        // Setup compilation arguments.
        const std::wstring entryPoint = [=]()
        {
            switch (shaderType)
            {
                case ShaderTypes::Vertex:
                    {
                        return L"VsMain";
                    }
                    break;

                case ShaderTypes::Pixel:
                    {
                        return L"PsMain";
                    }
                    break;

                default:
                    {
                        return L"";
                    }
                    break;
            }
        }();

        const std::wstring targetProfile = [=]()
        {
            switch (shaderType)
            {
                case ShaderTypes::Vertex:
                    {
                        return L"vs_6_6";
                    }
                    break;

                case ShaderTypes::Pixel:
                    {
                        return L"ps_6_6";
                    }
                    break;

                default:
                    {
                        return L"";
                    }
                    break;
            }
        }();

        std::vector<LPCWSTR> compilationArguments{
            L"-E",
            entryPoint.c_str(),
            L"-T",
            targetProfile.c_str(),
            DXC_ARG_PACK_MATRIX_ROW_MAJOR,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            DXC_ARG_ALL_RESOURCES_BOUND,
        };

        // Indicate that the shader should be in a debuggable state if in debug mode.
        // Else, set optimization level to 03.
        if constexpr (NETHER_DEBUG_MODE)
        {
            compilationArguments.push_back(DXC_ARG_DEBUG);
        }
        else
        {
            compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
        }

        // Load the shader source file to a blob.
        Comptr<IDxcBlobEncoding> sourceBlob{};
        throwIfFailed(utils->LoadFile(shaderPath.data(), nullptr, &sourceBlob));

        DxcBuffer sourceBuffer{
            .Ptr = sourceBlob->GetBufferPointer(),
            .Size = sourceBlob->GetBufferSize(),
            .Encoding = 0u,
        };

        // Compile the shader.
        Microsoft::WRL::ComPtr<IDxcResult> compiledShaderBuffer{};
        const HRESULT hr = compiler->Compile(&sourceBuffer,
                                             compilationArguments.data(),
                                             static_cast<uint32_t>(compilationArguments.size()),
                                             includeHandler.Get(),
                                             IID_PPV_ARGS(&compiledShaderBuffer));
        if (FAILED(hr))
        {
            fatalError(std::wstring(L"Failed to compile shader with path : ") + shaderPath.data());
        }

        // Get compilation errors (if any).
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
        throwIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
        if (errors && errors->GetStringLength() > 0)
        {
            const LPCSTR errorMessage = errors->GetStringPointer();
            fatalError(errorMessage);
        }

        // Get shader reflection data.
        Comptr<IDxcBlob> reflectionBlob{};
        throwIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr));

        const DxcBuffer reflectionBuffer{
            .Ptr = reflectionBlob->GetBufferPointer(),
            .Size = reflectionBlob->GetBufferSize(),
            .Encoding = 0,
        };

        Comptr<ID3D12ShaderReflection> shaderReflection{};
        utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
        D3D12_SHADER_DESC shaderDesc{};
        shaderReflection->GetDesc(&shaderDesc);

        // Helper lambda function : Given a mask, return the underlying data format.
        auto maskToFormat = [=](uint32_t mask)
        {
            switch (mask)
            {
                case 7:
                    {
                        return DXGI_FORMAT_R32G32B32_FLOAT;
                    }
                    break;

                case 3:
                    {
                        return DXGI_FORMAT_R32G32_FLOAT;
                    }
                    break;
            }

            return DXGI_FORMAT_UNKNOWN;
        };

        // Setup the input assembler if used. Only applicable for vertex shaders currently.
        if (shaderType == ShaderTypes::Vertex)
        {
            pipelineReflectionData.inputElementSemanticNames.reserve(shaderDesc.InputParameters);
            pipelineReflectionData.inputElementDescs.reserve(shaderDesc.InputParameters);

            for (const uint32_t parameterIndex : std::views::iota(0u, shaderDesc.InputParameters))
            {
                D3D12_SIGNATURE_PARAMETER_DESC signatureParameterDesc{};
                shaderReflection->GetInputParameterDesc(parameterIndex, &signatureParameterDesc);

                pipelineReflectionData.inputElementSemanticNames.emplace_back(signatureParameterDesc.SemanticName);

                pipelineReflectionData.inputElementDescs.emplace_back(D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = pipelineReflectionData.inputElementSemanticNames.back().c_str(),
                    .SemanticIndex = signatureParameterDesc.SemanticIndex,
                    .Format = maskToFormat(signatureParameterDesc.Mask),
                    .InputSlot = 0u,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, // There doesn't seem to be a obvious way to automate this currently.
                    .InstanceDataStepRate = 0u,
                });
            }

            pipelineReflectionData.inputLayoutDesc = {
                .pInputElementDescs = pipelineReflectionData.inputElementDescs.data(),
                .NumElements = static_cast<uint32_t>(pipelineReflectionData.inputElementDescs.size()),
            };
        }

        for (const uint32_t i : std::views::iota(0u, shaderDesc.BoundResources))
        {
            D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
            throwIfFailed(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));

            if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER)
            {
                pipelineReflectionData.rootParameterIndexMap[stringToWString(shaderInputBindDesc.Name)] = static_cast<uint32_t>(pipelineReflectionData.rootParameters.size());
                ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
                D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
                shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

                const D3D12_ROOT_PARAMETER1 rootParameter{
                    .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
                    .Descriptor{
                        .ShaderRegister = shaderInputBindDesc.BindPoint,
                        .RegisterSpace = shaderInputBindDesc.Space,
                        .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                    },
                };

                pipelineReflectionData.rootParameters.push_back(rootParameter);
            }
            else if (shaderInputBindDesc.Type == D3D_SIT_TEXTURE)
            {
                // For now, each individual texture belongs in its own descriptor table. This can cause the root signature to quickly exceed the 64WORD size limit.
                pipelineReflectionData.rootParameterIndexMap[stringToWString(shaderInputBindDesc.Name)] = static_cast<uint32_t>(pipelineReflectionData.rootParameters.size());
                const CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                                         1u,
                                                         shaderInputBindDesc.BindPoint,
                                                         shaderInputBindDesc.Space,
                                                         D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
                pipelineReflectionData.descriptorRanges.push_back(srvRange);

                const D3D12_ROOT_PARAMETER1 rootParameter{
                    .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                    .DescriptorTable =
                        {
                            .NumDescriptorRanges = 1u,
                            .pDescriptorRanges = &pipelineReflectionData.descriptorRanges.back(),
                        },
                    .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
                };

                pipelineReflectionData.rootParameters.push_back(rootParameter);
            }
        }

        Comptr<IDxcBlob> compiledShaderBlob{nullptr};
        compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);

        shader.shaderBlob = compiledShaderBlob;
        return shader;
    };

    Shader compile(const ShaderTypes& shaderType, const std::wstring_view shaderPath)
    {
        Shader shader{};

        if (!utils)
        {
            throwIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
            throwIfFailed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
            throwIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));
        }

        // Setup compilation arguments.
        const std::wstring entryPoint = [=]()
        {
            switch (shaderType)
            {
                case ShaderTypes::Vertex:
                    {
                        return L"VsMain";
                    }
                    break;

                case ShaderTypes::Pixel:
                    {
                        return L"PsMain";
                    }
                    break;

                case ShaderTypes::Compute:
                    {
                        return L"CsMain";
                    }
                    break;

                default:
                    {
                        return L"";
                    }
                    break;
            }
        }();

        const std::wstring targetProfile = [=]()
        {
            switch (shaderType)
            {
                case ShaderTypes::Vertex:
                    {
                        return L"vs_6_6";
                    }
                    break;

                case ShaderTypes::Pixel:
                    {
                        return L"ps_6_6";
                    }
                    break;

                    case ShaderTypes::Compute:
                    {
                        return L"cs_6_6";
                    }
                    break;

                default:
                    {
                        return L"";
                    }
                    break;
            }
        }();

        std::vector<LPCWSTR> compilationArguments{
            L"-E",
            entryPoint.c_str(),
            L"-T",
            targetProfile.c_str(),
            DXC_ARG_PACK_MATRIX_ROW_MAJOR,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            DXC_ARG_ALL_RESOURCES_BOUND,
        };

        // Indicate that the shader should be in a debuggable state if in debug mode.
        // Else, set optimization level to 03.
        if constexpr (NETHER_DEBUG_MODE)
        {
            compilationArguments.push_back(DXC_ARG_DEBUG);
        }
        else
        {
            compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
        }

        // Load the shader source file to a blob.
        Comptr<IDxcBlobEncoding> sourceBlob{};
        throwIfFailed(utils->LoadFile(shaderPath.data(), nullptr, &sourceBlob));

        DxcBuffer sourceBuffer{
            .Ptr = sourceBlob->GetBufferPointer(),
            .Size = sourceBlob->GetBufferSize(),
            .Encoding = 0u,
        };

        // Compile the shader.
        Microsoft::WRL::ComPtr<IDxcResult> compiledShaderBuffer{};
        const HRESULT hr = compiler->Compile(&sourceBuffer,
                                             compilationArguments.data(),
                                             static_cast<uint32_t>(compilationArguments.size()),
                                             includeHandler.Get(),
                                             IID_PPV_ARGS(&compiledShaderBuffer));
        if (FAILED(hr))
        {
            fatalError(std::wstring(L"Failed to compile shader with path : ") + shaderPath.data());
        }

        // Get compilation errors (if any).
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
        throwIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
        if (errors && errors->GetStringLength() > 0)
        {
            const LPCSTR errorMessage = errors->GetStringPointer();
            fatalError(errorMessage);
        }

        Comptr<IDxcBlob> compiledShaderBlob{nullptr};
        compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);

        shader.shaderBlob = compiledShaderBlob;
        return shader;
    }
}
