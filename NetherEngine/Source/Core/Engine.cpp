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
		ImGuiIO& io = ImGui::GetIO(); (void)io;

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

		const DirectX::XMVECTOR eyePosition = DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f);
		const DirectX::XMVECTOR focusPosition = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		const DirectX::XMVECTOR upDirection = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		static float rotation = 0.0f;
		rotation += 1.0f * deltaTime;

		mMvpBuffer = 
		{
			.modelMatrix = DirectX::XMMatrixRotationZ(rotation) * DirectX::XMMatrixRotationX(rotation) * DirectX::XMMatrixRotationY(-rotation),
			.viewProjectionMatrix = mCamera.GetViewMatrix() * DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), mClientDimensions.x / (float)mClientDimensions.y, 0.1f, 1000.0f)
		};

		mCube->GetTransformConstantBuffer()->Update(&mMvpBuffer);

		mMvpBuffer =
		{
			.modelMatrix = DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f),
			.viewProjectionMatrix = mCamera.GetViewMatrix() * DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), mClientDimensions.x / (float)mClientDimensions.y, 0.1f, 1000.0f)
		};

		mSponza->GetTransformConstantBuffer()->Update(&mMvpBuffer);
	}

	void Engine::Render()
	{
		mDevice->StartFrame();
		
		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Camera Settings");
		ImGui::SliderFloat("Movement Speed", &mCamera.mMovementSpeed, 0.1f, 1000.0f);
		ImGui::SliderFloat("Rotation Speed", &mCamera.mRotationSpeed, 0.1f, 10.0f);
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

		const size_t cubeIndicesCount = mCube->GetIndicesCount();

		for (size_t i : std::views::iota(0u, cubeIndicesCount))
		{
			commandList->IASetVertexBuffers(0u, 1u, &mCube->mVertexBuffers[i].vertexBufferView);
			commandList->IASetIndexBuffer(&mCube->mIndexBuffers[i].indexBufferView);

			commandList->SetGraphicsRootConstantBufferView(0u, mCube->GetTransformConstantBuffer()->resource->GetGPUVirtualAddress());
			
			commandList->DrawIndexedInstanced(mCube->mIndexBuffers[i].indicesCount, 1u, 0u, 0u, 0u);
		}

		const size_t sponzaIndicesCount = mSponza->GetIndicesCount();

		for (size_t i : std::views::iota(0u, sponzaIndicesCount))
		{
			commandList->IASetVertexBuffers(0u, 1u, &mSponza->mVertexBuffers[i].vertexBufferView);
			commandList->IASetIndexBuffer(&mSponza->mIndexBuffers[i].indexBufferView);

			commandList->SetGraphicsRootConstantBufferView(0u, mSponza->GetTransformConstantBuffer()->resource->GetGPUVirtualAddress());

			commandList->DrawIndexedInstanced(mSponza->mIndexBuffers[i].indicesCount, 1u, 0u, 0u, 0u);
		}

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

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

		mAssetsDirectoryPath = currentDirectory.wstring() + L"/Assets/";
	}

	std::wstring Engine::GetAssetPath(const std::wstring_view assetPath) const
	{
		return std::move(mAssetsDirectoryPath + assetPath.data());
	}

	void Engine::LoadContentAndAssets()
	{
		rendering::Shader vertexShader = rendering::ShaderCompiler::GetInstance().CompilerShader(rendering::ShaderType::Vertex, GetAssetPath(L"Shaders/HelloTriangleShader.hlsl"));
		rendering::Shader pixelShader =  rendering::ShaderCompiler::GetInstance().CompilerShader(rendering::ShaderType::Pixel, GetAssetPath(L"Shaders/HelloTriangleShader.hlsl"));

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

		const D3D12_VERSIONED_ROOT_SIGNATURE_DESC  rootSignatureDesc
		{
			.Version = featureData.HighestVersion,
			.Desc_1_1
			{
				.NumParameters = static_cast<uint32_t>(vertexShader.shaderReflection.rootParameters.size()),
				.pParameters = vertexShader.shaderReflection.rootParameters.data(),
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			}
		};

		Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob{};
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob{};
		utils::ThrowIfFailed(::D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &rootSignatureBlob, &errorBlob));
		utils::ThrowIfFailed(mDevice->GetDevice()->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mHelloTriangleRootSignature)));

		const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc = ConstructInputElementDesc(vertexShader.shaderReflection.inputElementDescs);
		// temp : vertexShader.shaderReflection.rootParameters.insert(vertexShader.shaderReflection.rootParameters.end(), pixelShader.shaderReflection.rootParameters.begin(), pixelShader.shaderReflection.rootParameters.end());


		// Create graphics pipeline state.
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc
		{
			.pRootSignature = mHelloTriangleRootSignature.Get(),
			.VS
			{
				.pShaderBytecode = vertexShader.shaderBlob->GetBufferPointer(),
				.BytecodeLength = vertexShader.shaderBlob->GetBufferSize()
            },
			.PS
			{
				.pShaderBytecode = pixelShader.shaderBlob->GetBufferPointer(),
				.BytecodeLength = pixelShader.shaderBlob->GetBufferSize()
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
		
		// Create Model.
		mCube = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/Box/glTF/Box.gltf"), L"Cube Model");
		mSponza = std::make_unique<scene::GameObject>(mDevice.get(), GetAssetPath(L"Models/sponza-gltf-pbr/sponza.glb"), L"Sponza Model");

		mDevice->GetDirectCommandQueue()->Flush(mDevice->GetCurrentSwapChainBackBufferIndex());
	}
}