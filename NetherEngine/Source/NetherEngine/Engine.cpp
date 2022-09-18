#include "Pch.hpp"

#include "Engine.hpp"

#include "DataTypes.hpp"
#include "Utils.hpp"
#include "Shader.hpp"
#include "DescriptorHeap.hpp"

namespace nether
{
	Engine::~Engine()
	{
		Destroy();
	}
	
	void Engine::Init(const HWND windowHandle, const Uint2& clientDimensions)
	{
		FindAssetsDirectory();

		LoadCoreObjects(windowHandle, clientDimensions);
		LoadContentAndAssets();

		Flush();

		mIsInitialized = true;
	}

	void Engine::Update(float deltaTime)
	{
		const DirectX::XMVECTOR eyePosition = DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f);
		const DirectX::XMVECTOR focusPosition = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		const DirectX::XMVECTOR upDirection = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		static float rotation = 0.0f;
		rotation += 1.0f * deltaTime;

		mMvpBuffer = 
		{
			.modelMatrix = DirectX::XMMatrixRotationZ(rotation) * DirectX::XMMatrixRotationX(rotation) * DirectX::XMMatrixRotationY(-rotation),
			.viewProjectionMatrix = DirectX::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection) * DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), mClientDimensions.x / (float)mClientDimensions.y, 0.1f, 1000.0f)
		};

		mConstantBuffer->Update(&mMvpBuffer);
	}

	void Engine::Render()
	{
		StartFrame();

		const CD3DX12_RESOURCE_BARRIER backBufferPresentToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentSwapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1u, &backBufferPresentToRenderTarget);

		static constexpr std::array<float, 4> clearColor{ 0.2f, 0.2f, 0.2f, 1.0f };
		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvDescriptorHeap->GetDescriptorHandleFromHeapStart().cpuDescriptorHandle, mCurrentSwapChainBackBufferIndex, mRtvDescriptorHeap->GetDescriptorHandleSize());
		const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDsvDescriptorHeap->GetDescriptorHandleFromHeapStart().cpuDescriptorHandle);

		mCommandList->OMSetRenderTargets(1u, &rtvHandle, FALSE, &dsvHandle);
		mCommandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);
		mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 1u, 0u, nullptr);

		mCommandList->RSSetScissorRects(1u, &mScissorRect);
		mCommandList->RSSetViewports(1u, &mViewport);

		mCommandList->SetPipelineState(mHelloTrianglePipelineState.Get());
		mCommandList->SetGraphicsRootSignature(mHelloTriangleRootSignature.Get());

		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (size_t i : std::views::iota(0u, mCube->mIndexBuffers.size()))
		{
			mCommandList->IASetVertexBuffers(0u, 1u, &mCube->mVertexBuffers[i]->vertexBufferView);
			mCommandList->IASetIndexBuffer(&mCube->mIndexBuffers[i]->indexBufferView);

			mCommandList->SetGraphicsRootConstantBufferView(0u, mConstantBuffer->resource->GetGPUVirtualAddress());
			
			mCommandList->DrawIndexedInstanced(mCube->mIndexBuffers[i]->indicesCount, 1u, 0u, 0u, 0u);
		}

		const CD3DX12_RESOURCE_BARRIER backBufferRenderTargetToPresent = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentSwapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1u, &backBufferRenderTargetToPresent);

		ExecuteCommandList();

		Present();
		EndFrame();
	}

	void Engine::Destroy()
	{
		Flush();
	}

	void Engine::Resize(const Uint2& clientDimensions)
	{
		if (clientDimensions != mClientDimensions)
		{
			// Flush the queue to execute all pending operations, reset swapchain back buffers, resize the buffer's, and create the RTV's again.
			Flush();

			mClientDimensions = clientDimensions;

			// Don't allow swapchain back buffers to have width / height as 0u.
			mClientDimensions.x = std::max<uint32_t>(mClientDimensions.x, 1u);
			mClientDimensions.y = std::max<uint32_t>(mClientDimensions.y, 1u);

			for (const uint32_t bufferIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
			{
				mSwapChainBackBuffers[bufferIndex].Reset();
				mFrameFenceValues[bufferIndex] = mFrameFenceValues[mCurrentSwapChainBackBufferIndex];
			}

			DXGI_SWAP_CHAIN_DESC swapChainDesc{};
			utils::ThrowIfFailed(mSwapChain->GetDesc(&swapChainDesc));
			utils::ThrowIfFailed(mSwapChain->ResizeBuffers(NUMBER_OF_FRAMES, mClientDimensions.x, mClientDimensions.y, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

			mCurrentSwapChainBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

			CreateRenderTargetViews();
			CreateDepthStencilBuffer();
		}
	}

	void Engine::OnKeyAction(const uint8_t keycode, const bool isKeyDown)
	{
		if (isKeyDown && keycode == VK_F5)
		{
			mVsync = true;
		}
		else if (isKeyDown && keycode == VK_F6)
		{
			mVsync = false;
		}
	}

	void Engine::StartFrame()
	{
		utils::ThrowIfFailed(mCommandAllocators[mCurrentSwapChainBackBufferIndex]->Reset());
		utils::ThrowIfFailed(mCommandList->Reset(mCommandAllocators[mCurrentSwapChainBackBufferIndex].Get(), nullptr));
	}

	void Engine::Present()
	{
		const UINT syncInterval = mVsync ? 1u : 0u;
		const UINT presentFlags = !mVsync && mTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0u;

		mSwapChain->Present(syncInterval, presentFlags);
	}

	void Engine::EndFrame()
	{
		mFrameFenceValues[mCurrentSwapChainBackBufferIndex] = Signal();
		
		mCurrentSwapChainBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();
		
		WaitForFenceValue(mFrameFenceValues[mCurrentSwapChainBackBufferIndex]);
	}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> Engine::GetCommandList()
	{
		utils::ThrowIfFailed(mCommandList->Close());
		utils::ThrowIfFailed(mCommandList->Reset(mCommandAllocators[mCurrentSwapChainBackBufferIndex].Get(), nullptr));

		return mCommandList;
	}

	void Engine::ExecuteCommandList()
	{
		mCommandList->Close();

		std::array<ID3D12CommandList* const, 1u> commandList
		{
			mCommandList.Get()
		};

		mDirectCommandQueue->ExecuteCommandLists(1u, commandList.data());
	}

	uint64_t Engine::Signal()
	{
		const uint64_t fenceValueToSignal = ++mFenceValue;
		utils::ThrowIfFailed(mDirectCommandQueue->Signal(mFence.Get(), fenceValueToSignal));

		return fenceValueToSignal;
	}

	void Engine::WaitForFenceValue(const uint64_t fenceValue)
	{
		if (mFence->GetCompletedValue() < fenceValue)
		{
			utils::ThrowIfFailed(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
			::WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
		}
	}

	void Engine::Flush()
	{
		const uint64_t fenceValue = Signal();
		WaitForFenceValue(fenceValue);
		mFrameFenceValues[mCurrentSwapChainBackBufferIndex] = fenceValue;
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

	void Engine::LoadCoreObjects(const HWND windowHandle, const Uint2& clientDimensions)
	{
		mClientDimensions = clientDimensions;

		// Enable the debug layer if in debug build configuration, and set the factory create flag to DXGI_CREATE_FACTORY_DEBUG.
		UINT dxgiFactoryCreateFlags = 0;

		if constexpr (NETHER_DEBUG_BUILD)
		{
			utils::ThrowIfFailed(::D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&mDebugController)));
			mDebugController->EnableDebugLayer();
			mDebugController->SetEnableGPUBasedValidation(TRUE);
			mDebugController->SetEnableSynchronizedCommandQueueValidation(TRUE);
			mDebugController->SetEnableAutoName(TRUE);

			dxgiFactoryCreateFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}

		// Create DXGI factory so adapters can be queried, and DXGI objects can be generated.
		utils::ThrowIfFailed(::CreateDXGIFactory2(dxgiFactoryCreateFlags, IID_PPV_ARGS(&mFactory)));

		// Get the adapter with best performance (adapter represents a display system (GPU's, Video Memory, etc).
		utils::ThrowIfFailed(mFactory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&mAdapter)));

		// Log the adapter name if in debug mode.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			DXGI_ADAPTER_DESC adapterDesc{};
			utils::ThrowIfFailed(mAdapter->GetDesc(&adapterDesc));
			::OutputDebugStringW(adapterDesc.Description);
		}

		// Create D3D12 device (a virtual adapter, for creating most D3D12 objects).
		utils::ThrowIfFailed(::D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice)));
		utils::SetName(mDevice.Get(), L"D3D12 Device");

		// Set break points on error messages / warning messages / corruption messages in debug builds.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue{};
			utils::ThrowIfFailed(mDevice.As(&infoQueue));

			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		}

		// Create direct command queue (execution port of GPU -> has methods for submitting command list and their execution).
		const D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0u,
		};

		utils::ThrowIfFailed(mDevice->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(&mDirectCommandQueue)));
		utils::SetName(mDirectCommandQueue.Get(), L"Direct Command Queue");

		// Check for tearing support (must be supported by both the monitor and the adapter).
		BOOL tearingSupported{ FALSE };
		if (SUCCEEDED(mFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported, sizeof(tearingSupported))))
		{
			mTearingSupported = true;
		}

		// Create the swapchain (implements surfaces for rendering data before presenting it to a output).
		const DXGI_SWAP_CHAIN_DESC1 swapChainDesc
		{
			.Width = mClientDimensions.x,
			.Height = mClientDimensions.y,
			.Format = SWAP_CHAIN_BACK_BUFFER_FORMAT,
			.Stereo = FALSE,
			.SampleDesc = {1u, 0u},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = NUMBER_OF_FRAMES,
			.Scaling = DXGI_SCALING_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
			.Flags = mTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
		};

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain{};
		utils::ThrowIfFailed(mFactory->CreateSwapChainForHwnd(mDirectCommandQueue.Get(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
		utils::ThrowIfFailed(mFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));
		utils::ThrowIfFailed(swapChain.As(&mSwapChain));

		mCurrentSwapChainBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		// Create descriptor heaps (contiguous allocation of resource descriptors).
		const D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc
		{
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 10u,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0u,
		};

		mRtvDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 5u, L"RTV Descriptor Heap");
		mDsvDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 5u, L"DSV Descriptor Heap");
		mCbvSrvUavDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5u, L"CBV SRV UAV Descriptor Heap");
		mSamplerDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 5u, L"Sampler Descriptor Heap");

		CreateRenderTargetViews();
		CreateDepthStencilBuffer();

		// Make viewport.
		mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)mClientDimensions.x, (float)mClientDimensions.y);

		// Create command allocators.
		for (const uint32_t frameIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[frameIndex]));
			utils::SetName(mCommandAllocators[frameIndex].Get(), L"Command Allocator");
		}

		// Create command list.
		mDevice->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[mCurrentSwapChainBackBufferIndex].Get(), nullptr, IID_PPV_ARGS(&mCommandList));
		utils::SetName(mCommandList.Get(), L"Command List");
		utils::ThrowIfFailed(mCommandList->Close());

		// Create synchronization primitives.
		utils::ThrowIfFailed(mDevice->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		utils::SetName(mFence.Get(), L"Direct Command Queue Fence");

		mFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!mFenceEvent)
		{
			utils::ErrorMessage(L"Failed to create fence event.");
		}
	}

	void Engine::LoadContentAndAssets()
	{
		Shader vertexShader = ShaderCompiler::GetInstance().CompilerShader(ShaderType::Vertex, GetAssetPath(L"Shaders/HelloTriangleShader.hlsl"));
		Shader pixelShader =  ShaderCompiler::GetInstance().CompilerShader(ShaderType::Pixel, GetAssetPath(L"Shaders/HelloTriangleShader.hlsl"));

		// Create root signature.
		// Get the highest supported root signature version.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData
		{
			.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0,
		};

		if (SUCCEEDED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
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
		utils::ThrowIfFailed(mDevice->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mHelloTriangleRootSignature)));

		const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc = utils::ConstructInputElementDesc(vertexShader.shaderReflection.inputElementDescs);
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
			.RTVFormats = {SWAP_CHAIN_BACK_BUFFER_FORMAT},
			.DSVFormat = {DXGI_FORMAT_D32_FLOAT},
			.SampleDesc = {1u, 0u},
			.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
		};

		utils::ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&mHelloTrianglePipelineState)));
		
		// Create Model.
		mCube = std::make_unique<Model>(GetAssetPath(L"Models/Box/glTF/Box.gltf"), this);

		ConstantBufferCreationDesc constantBufferCreationDesc
		{
			.bufferSize = sizeof(MVPBuffer)
		};

		mConstantBuffer = CreateConstantBuffer(constantBufferCreationDesc, L"Constant Buffer");

		Flush();
	}

	void Engine::CreateRenderTargetViews()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuDescriptorHandle = mRtvDescriptorHeap->GetDescriptorHandleFromHeapStart().cpuDescriptorHandle;
		
		// Create RTV's for the swapchain back buffers.
		for (const uint32_t bufferIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			mSwapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(mSwapChainBackBuffers[bufferIndex].ReleaseAndGetAddressOf()));
			mDevice->CreateRenderTargetView(mSwapChainBackBuffers[bufferIndex].Get(), nullptr, rtvCpuDescriptorHandle);

			// Offset descriptor handle.
			rtvCpuDescriptorHandle.ptr += mRtvDescriptorHeap->GetDescriptorHandleSize();
		}

		mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)mClientDimensions.x, (float)mClientDimensions.y);
		mAspectRatio = (float)mClientDimensions.x / mClientDimensions.y;
	}

	void Engine::CreateDepthStencilBuffer()
	{
		// Create DSV buffer.
		const D3D12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		const CD3DX12_RESOURCE_DESC depthStencilResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, mClientDimensions.x, mClientDimensions.y, 1u, 0u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		const D3D12_CLEAR_VALUE optimizedClearValue
		{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil
			{
				.Depth = 1.0f
			}
		};

		utils::ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&mDepthStencilBuffer)));
		utils::SetName(mDepthStencilBuffer.Get(), L"Depth Stencil Buffer");

		D3D12_CPU_DESCRIPTOR_HANDLE dsvCPUDescriptorHandle = mDsvDescriptorHeap->GetDescriptorHandleFromHeapStart().cpuDescriptorHandle;

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc
		{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE,
			.Texture2D
			{
				.MipSlice = 0u
            }
		};

		mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &depthStencilViewDesc, dsvCPUDescriptorHandle);
	}

	std::unique_ptr<VertexBuffer> Engine::CreateVertexBuffer(const VertexBufferCreationDesc& vertexBufferCreationDesc, void* data, const std::wstring_view bufferName)
	{
		if (!data)
		{
			utils::ErrorMessage(L"Cannot create vertex buffer with no data.");
		}
;
		const size_t bufferSize = vertexBufferCreationDesc.numberOfElements * vertexBufferCreationDesc.stride;
		
		// Create destination resource (on GPU only memory).
		Microsoft::WRL::ComPtr<ID3D12Resource> destinationResource{};

		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC destinationResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);

		utils::ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &destinationResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&destinationResource)));

		// Create intermediate resource (to copy data from CPU - GPU shared memory to GPU only memory).
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource{};
		
		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC intermediateResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);

		utils::ThrowIfFailed(mDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &intermediateResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource)));
		utils::SetName(intermediateResource.Get(), L"Vertex Buffer Intermediate resource");

		const D3D12_SUBRESOURCE_DATA subresourceData
		{
			.pData = data,
			.RowPitch = static_cast<LONG_PTR>(bufferSize),
			.SlicePitch = static_cast<LONG_PTR>(bufferSize)
		};

		utils::ThrowIfFailed(mCommandList->Reset(mCommandAllocators[mCurrentSwapChainBackBufferIndex].Get(), nullptr));
		UpdateSubresources(mCommandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0u, 0u, 1u, &subresourceData);
		ExecuteCommandList();
		Flush();

		// Create vertex buffer view.
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView
		{
			.BufferLocation = destinationResource->GetGPUVirtualAddress(),
			.SizeInBytes = static_cast<UINT>(bufferSize),
			.StrideInBytes = vertexBufferCreationDesc.stride
		};

		// Create vertex buffer
		const VertexBuffer vertexBuffer
		{
			.resource = destinationResource.Get(),
			.vertexBufferView = std::move(vertexBufferView)
		};

		utils::SetName(vertexBuffer.resource.Get(), bufferName.data());
		return std::move(std::make_unique<VertexBuffer>(vertexBuffer));
	}

	std::unique_ptr<IndexBuffer> Engine::CreateIndexBuffer(const IndexBufferCreationDesc& indexBufferCreationDesc, void* data, const std::wstring_view bufferName)
	{
		if (!data)
		{
			utils::ErrorMessage(L"Cannot create index buffer with no data.");
		}
		
		const size_t bufferSize = indexBufferCreationDesc.bufferSize;

		// Create destination resource (on GPU only memory).
		Microsoft::WRL::ComPtr<ID3D12Resource> destinationResource{};

		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC destinationResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);

		utils::ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &destinationResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&destinationResource)));

		// Create intermediate resource (to copy data from CPU - GPU shared memory to GPU only memory).
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource{};

		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC intermediateResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);

		utils::ThrowIfFailed(mDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &intermediateResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource)));
		utils::SetName(intermediateResource.Get(), L"Index Buffer Intermediate resource");

		const D3D12_SUBRESOURCE_DATA subresourceData
		{
			.pData = data,
			.RowPitch = static_cast<LONG_PTR>(bufferSize),
			.SlicePitch = static_cast<LONG_PTR>(bufferSize)
		};

		utils::ThrowIfFailed(mCommandList->Reset(mCommandAllocators[mCurrentSwapChainBackBufferIndex].Get(), nullptr));
		UpdateSubresources(mCommandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0u, 0u, 1u, &subresourceData);
		ExecuteCommandList();
		Flush();

		// Create index buffer view.
		D3D12_INDEX_BUFFER_VIEW indexBufferView
		{
			.BufferLocation = destinationResource->GetGPUVirtualAddress(),
			.SizeInBytes = static_cast<UINT>(bufferSize),
			.Format = indexBufferCreationDesc.format
		};

		// Create index buffer
		const IndexBuffer indexBuffer
		{
			.resource = destinationResource.Get(),
			.indicesCount = indexBufferCreationDesc.indicesCount,
			.indexBufferView = std::move(indexBufferView)
		};

		utils::SetName(indexBuffer.resource.Get(), bufferName.data());
		return std::move(std::make_unique<IndexBuffer>(indexBuffer));
	}

	std::unique_ptr<ConstantBuffer> Engine::CreateConstantBuffer(const ConstantBufferCreationDesc& constantBufferCreationDesc, const std::wstring_view bufferName)
	{
		ConstantBuffer constantBuffer{};
		constantBuffer.bufferSize = constantBufferCreationDesc.bufferSize;

		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RANGE readRangeNoCPUAccess{ 0, 0 };

		// Create constant buffer.
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferCreationDesc.bufferSize);
		utils::ThrowIfFailed(mDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &constantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer.resource)));
		utils::ThrowIfFailed(constantBuffer.resource->Map(0u, &readRangeNoCPUAccess, reinterpret_cast<void**>(&constantBuffer.mappedBufferPointer)));

		utils::SetName(constantBuffer.resource.Get(), bufferName);
		return std::move(std::make_unique<ConstantBuffer>(constantBuffer));
	}
}