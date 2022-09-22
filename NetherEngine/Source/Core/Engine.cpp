#include "Pch.hpp"

#include "Engine.hpp"

#include "Common/DataTypes.hpp"
#include "Common/Utils.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/DescriptorHeap.hpp"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

namespace nether::core
{
	Engine::~Engine()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		Destroy();
	}
	
	void Engine::Init(const HWND windowHandle, const Uint2& clientDimensions)
	{
		mWindowHandle = windowHandle;

		FindAssetsDirectory();

		mClientDimensions = clientDimensions;
		mAspectRatio = static_cast<float>(clientDimensions.x) / static_cast<float>(clientDimensions.y);

		mDevice = std::make_unique<rendering::Device>(windowHandle, clientDimensions);

		LoadContentAndAssets();

		// TEMP : ImGui stuff.
		// ImGui stuff.
		ImGui_ImplWin32_EnableDpiAwareness();

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// Not really working for now.
		/*
		const std::wstring iniFilePath = mRootDirectoryPath + L"imgui.ini";
		const std::filesystem::path relativeIniFilePath = std::filesystem::relative(iniFilePath);
		const std::string relativeFilePathStr = relativeIniFilePath.string();
		io.IniFilename = relativeFilePathStr.c_str();
		*/

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		rendering::Descriptor srvDescriptor = mDevice->GetCbvSrvUavDescriptorHeap()->GetCurrentDescriptorHeap();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(windowHandle);
		ImGui_ImplDX12_Init(mDevice->GetDevice(), rendering::NUMBER_OF_FRAMES, rendering::SWAP_CHAIN_BACK_BUFFER_FORMAT, mDevice->GetCbvSrvUavDescriptorHeap()->GetDescriptorHeap(),
			srvDescriptor.cpuDescriptorHandle, srvDescriptor.gpuDescriptorHandle);

		mDevice->GetDirectCommandQueue()->Flush(mDevice->GetCurrentSwapChainBackBufferIndex());

		mIsInitialized = true;
	}

	void Engine::Update(float deltaTime)
	{
		mCamera.Update(deltaTime);
	}

	void Engine::Render()
	{
		mDevice->StartFrame();
		
		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::DockSpaceOverViewport(ImGui::GetWindowViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

		ImGui::Begin("Camera Settings");
		ImGui::SliderFloat("Movement Speed", &mCamera.mMovementSpeed, 0.1f, 1000.0f);
		ImGui::SliderFloat("Rotation Speed", &mCamera.mRotationSpeed, 0.1f, 10.0f);
		ImGui::End();

		// Scene constant buffer data UI.
		ImGui::Begin("Scene Buffer");
		ImGui::ColorEdit3("Directional Light color", &mSceneConstantBufferData.directionalLightColor.x);
		ImGui::SliderFloat3("Directional light direction", &mSceneConstantBufferData.directionalLightDirection.x, -5.0f, 5.0f);
		ImGui::End();

		// Update scene constant buffer.
		mSceneConstantBuffer->Update(&mSceneConstantBufferData);

		// Update transform components for all models.
		ImGui::Begin("Game Objects");
		for (const auto& [name, gameObject] : mGameObjects)
		{
			if (ImGui::TreeNode(utils::WStringToString(name).c_str()))
			{
				ImGui::SliderFloat3("Translation", &gameObject->GetTransformComponent().translate.x, -25.0f, 25.0f);
				ImGui::SliderFloat3("Rotation", &gameObject->GetTransformComponent().rotate.x, 0.0f, 360.0f);
				ImGui::SliderFloat3("Scale", &gameObject->GetTransformComponent().scale.x, 0.1f, 100.0f);
				ImGui::TreePop();
			}
		
			const DirectX::XMVECTOR scalingVector = DirectX::XMLoadFloat3(&gameObject->GetTransformComponent().scale);
			const DirectX::XMVECTOR rotationVector = DirectX::XMLoadFloat3(&gameObject->GetTransformComponent().rotate);
			const DirectX::XMVECTOR translationVector = DirectX::XMLoadFloat3(&gameObject->GetTransformComponent().translate);

			const DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixScalingFromVector(scalingVector) * DirectX::XMMatrixRotationRollPitchYawFromVector(rotationVector) * DirectX::XMMatrixTranslationFromVector(translationVector);			
			
			MVPBuffer mvpBuffer =
			{
				.modelMatrix = modelMatrix,
				.inverseModelMatrix = DirectX::XMMatrixInverse(nullptr, modelMatrix),
				.viewProjectionMatrix = mCamera.GetViewMatrix() * DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), mClientDimensions.x / (float)mClientDimensions.y, 0.1f, 1000.0f)
			};

			gameObject->GetTransformConstantBuffer()->Update(&mvpBuffer);
		}
		ImGui::End();

		ImGui::Render();

		auto backBuffer = mDevice->GetCurrentBackBufferResource();
		auto rtvHandle = mDevice->GetCurrentBackBufferRtvHandle();
		auto commandList = mDevice->GetDirectCommandQueue()->GetCommandList(mDevice->GetDevice(), mDevice->GetCurrentSwapChainBackBufferIndex());

		const std::array<ID3D12DescriptorHeap* const, 1u> descriptrorHeaps
		{
			mDevice->GetCbvSrvUavDescriptorHeap()->GetDescriptorHeap()
		};

		commandList->SetDescriptorHeaps(1u, descriptrorHeaps.data());

		const CD3DX12_RESOURCE_BARRIER backBufferPresentToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1u, &backBufferPresentToRenderTarget);

		static constexpr std::array<float, 4> clearColor{ 0.2f, 0.2f, 0.2f, 1.0f };
		const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDevice->GetDsvDescriptorHeap()->GetDescriptorHandleFromHeapStart().cpuDescriptorHandle);

		commandList->OMSetRenderTargets(1u, &rtvHandle, FALSE, &dsvHandle);
		commandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 1u, 0u, nullptr);

		commandList->RSSetScissorRects(1u, &mDevice->GetScissorRect());
		commandList->RSSetViewports(1u, &mDevice->GetViewport());

		commandList->SetPipelineState(mHelloTrianglePipelineState.Get());
		commandList->SetGraphicsRootSignature(mHelloTriangleRootSignature.Get());
		
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (const auto& [modelName, gameObject] : mGameObjects)
		{
			const size_t meshCount = gameObject->mMeshCount;

			for (size_t i : std::views::iota(0u, meshCount))
			{
				commandList->IASetVertexBuffers(0u, 1u, &gameObject->mVertexBuffers[i].vertexBufferView);
				commandList->IASetIndexBuffer(&gameObject->mIndexBuffers[i].indexBufferView);

				commandList->SetGraphicsRootConstantBufferView(mShaderReflection.rootParameterBindingMap[L"mvpBuffer"], gameObject->GetTransformConstantBuffer()->resource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(mShaderReflection.rootParameterBindingMap[L"sceneBuffer"], mSceneConstantBuffer->resource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(mShaderReflection.rootParameterBindingMap[L"albedoTexture"], gameObject->mAlbedoTextures[gameObject->mMaterialIndices[i]].gpuDescriptorHandle);

				commandList->DrawIndexedInstanced(gameObject->mIndexBuffers[i].indicesCount, 1u, 0u, 0u, 0u);
			}
		}

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
		
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault(mWindowHandle, commandList.Get());
		}

		const CD3DX12_RESOURCE_BARRIER backBufferRenderTargetToPresent = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1u, &backBufferRenderTargetToPresent);

		mDevice->GetDirectCommandQueue()->ExecuteCommandLists(commandList, mDevice->GetCurrentSwapChainBackBufferIndex());

		mDevice->Present();
		mDevice->EndFrame();
	}

	void Engine::Destroy()
	{
		mDevice->GetDirectCommandQueue()->Flush(mDevice->GetCurrentSwapChainBackBufferIndex());
	}

	void Engine::Resize(const Uint2& clientDimensions)
	{
		if (clientDimensions != mClientDimensions)
		{
			mDevice->Resize(clientDimensions);
			mAspectRatio = static_cast<float>(clientDimensions.x) / static_cast<float>(clientDimensions.y);
		}
	}

	void Engine::OnKeyAction(const uint8_t keycode, const bool isKeyDown)
	{
		mCamera.HandleInput(keycode, isKeyDown);

		if (isKeyDown && keycode == VK_F5)
		{
			mDevice->SetVsync(true);
		}
		else if (isKeyDown && keycode == VK_F6)
		{
			mDevice->SetVsync(false);
		}
	}

	void Engine::FindAssetsDirectory()
	{
		std::filesystem::path currentDirectory = std::filesystem::current_path();

		while (!std::filesystem::exists(currentDirectory / "Assets"))
		{
			if (currentDirectory.has_parent_path())
			{
				currentDirectory = currentDirectory.parent_path();
			}
			else
			{
				utils::ErrorMessage(L"Assets Directory not found!");
			}
		}

		const std::filesystem::path assetsDirectory = currentDirectory / "Assets";

		if (!std::filesystem::is_directory(assetsDirectory))
		{
			utils::ErrorMessage(L"Assets Directory that was located is not a directory!");
		}

		mRootDirectoryPath = currentDirectory.wstring() + L"/";
		mAssetsDirectoryPath = currentDirectory.wstring() + L"/Assets/";
	}

	std::wstring Engine::GetAssetPath(const std::wstring_view assetPath) const
	{
		return std::move(mAssetsDirectoryPath + assetPath.data());
	}

	void Engine::LoadContentAndAssets()
	{
		const std::wstring baseShaderDirectory = GetAssetPath(L"Shaders/");
		Microsoft::WRL::ComPtr<IDxcBlob> vertexShader = rendering::ShaderCompiler::GetInstance().CompilerShader(rendering::ShaderType::Vertex, GetAssetPath(L"Shaders/TestShader.hlsl"), baseShaderDirectory, mShaderReflection);
		Microsoft::WRL::ComPtr<IDxcBlob> pixelShader =  rendering::ShaderCompiler::GetInstance().CompilerShader(rendering::ShaderType::Pixel, GetAssetPath(L"Shaders/TestShader.hlsl"), baseShaderDirectory, mShaderReflection);

		// Create root signature.
		// Get the highest supported root signature version.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData
		{
			.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0,
		};

		if (SUCCEEDED(mDevice->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		}

		constexpr std::array<D3D12_STATIC_SAMPLER_DESC, 1> staticSamplers
		{
			D3D12_STATIC_SAMPLER_DESC
			{
				.Filter = D3D12_FILTER_ANISOTROPIC,
				.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MipLODBias = 0,
				.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
				.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
				.MinLOD = 0.0f,
				.MaxLOD = D3D12_FLOAT32_MAX,
				.ShaderRegister = 0,
				.RegisterSpace = 0,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
			}
		};

		const D3D12_VERSIONED_ROOT_SIGNATURE_DESC  rootSignatureDesc
		{
			.Version = featureData.HighestVersion,
			.Desc_1_1
			{
				.NumParameters = static_cast<uint32_t>(mShaderReflection.rootParameters.size()),
				.pParameters = mShaderReflection.rootParameters.data(),
				.NumStaticSamplers = static_cast<uint32_t>(staticSamplers.size()),
				.pStaticSamplers = staticSamplers.data(),
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			}
		};

		Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob{};
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob{};
		if (FAILED(::D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &rootSignatureBlob, &errorBlob)))
		{
			const char *message = (const char*)errorBlob->GetBufferPointer();
			utils::ErrorMessage(message);
		}

		utils::ThrowIfFailed(mDevice->GetDevice()->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mHelloTriangleRootSignature)));

		const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc = ConstructInputElementDesc(mShaderReflection.inputElementDescs);

		// Create graphics pipeline state.
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc
		{
			.pRootSignature = mHelloTriangleRootSignature.Get(),
			.VS
			{
				.pShaderBytecode = vertexShader->GetBufferPointer(),
				.BytecodeLength = vertexShader->GetBufferSize()
            },
			.PS
			{
				.pShaderBytecode = pixelShader->GetBufferPointer(),
				.BytecodeLength = pixelShader->GetBufferSize()
            },
			.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
			.SampleMask = UINT_MAX,
			.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
			.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC_LESS_EQUAL, FALSE, 0u, 0u, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL),
			.InputLayout
			{
				.pInputElementDescs = inputElementDesc.data(),
				.NumElements = static_cast<uint32_t>(inputElementDesc.size()),
            },
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1u,
			.RTVFormats = {rendering::SWAP_CHAIN_BACK_BUFFER_FORMAT},
			.DSVFormat = {DXGI_FORMAT_D32_FLOAT},
			.SampleDesc = {1u, 0u},
			.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
		};

		utils::ThrowIfFailed(mDevice->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&mHelloTrianglePipelineState)));
		
		// Create Models.
		mGameObjects[L"Floor"] = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Floor/glTF/Cube.gltf"), L"Floor Model");
		mGameObjects[L"Floor"]->GetTransformComponent().scale = { 100.0f, 0.1f, 100.0f };
		mGameObjects[L"Floor"]->GetTransformComponent().translate = { 0.0f, -10.1f, 0.0f };

		mGameObjects[L"Cube"] = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Cube/glTF/Cube.gltf"), L"Cube Model");
		mGameObjects[L"Cube"]->GetTransformComponent().translate.x += 2.0f;
		mGameObjects[L"Cube"]->GetTransformComponent().rotate.y += 2.0f;
		
		mGameObjects[L"Cube1"] = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Cube/glTF/Cube.gltf"), L"Cube Model");
		mGameObjects[L"Cube1"]->GetTransformComponent().translate.x += 7.0f;
		mGameObjects[L"Cube1"]->GetTransformComponent().rotate.z += 7.0f;

		mGameObjects[L"Cube2"] = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Cube/glTF/Cube.gltf"), L"Cube Model");
		mGameObjects[L"Cube2"]->GetTransformComponent().translate.y += 2.2f;
		mGameObjects[L"Cube2"]->GetTransformComponent().rotate.z += 2.2f;

		mGameObjects[L"Cube3"] = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Cube/glTF/Cube.gltf"), L"Cube Model");
		mGameObjects[L"Cube3"]->GetTransformComponent().translate.z += 7.0f;

		mGameObjects[L"Cube4"] = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Cube/glTF/Cube.gltf"), L"Cube Model");
		mGameObjects[L"Cube4"]->GetTransformComponent().translate.x -= 9.0f;
		mGameObjects[L"Cube4"]->GetTransformComponent().rotate.y -= 9.0f;

		// Create constant buffers.
		rendering::ConstantBufferCreationDesc sceneConstantBufferCreationDesc
		{
			.bufferSize = sizeof(SceneConstantBufferData)
		};

		mSceneConstantBuffer = std::make_unique<rendering::ConstantBuffer>(mDevice->CreateConstantBuffer(sceneConstantBufferCreationDesc, L"Scene Constant Buffer Data"));

		mDevice->GetDirectCommandQueue()->Flush(mDevice->GetCurrentSwapChainBackBufferIndex());
	}
}