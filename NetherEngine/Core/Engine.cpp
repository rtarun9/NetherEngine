#include "Pch.hpp"

#include "Engine.hpp"

#include "Graphics/Shader.hpp"

namespace nether::core
{
	Engine::~Engine()
	{
		Destroy();
	}
	
	void Engine::Init(const HWND windowHandle, const Uint2& clientDimensions)
	{
		FindAssetsDirectory();

		mWindowHandle = windowHandle;

		mClientDimensions = clientDimensions;
	
		LoadCoreObjects();
		LoadContentAndAssets();

		mIsInitialized = true;
	}

	void Engine::Update(const float deltaTime)
	{
	}

	void Engine::Render()
	{
		// Reset command allocator and list.
		ThrowIfFailed(mDirectCommandQueue.GetCommandAllocator(mCurrentBackBufferIndex)->Reset());

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList = mDirectCommandQueue.GetCommandList(mCurrentBackBufferIndex);

		// Clear RTV and DSV.
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvDescriptorHeap.GetDescriptorHandleFromIndex(mCurrentBackBufferIndex).cpuDescriptorHandle;
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDsvDescriptorHeap.GetDescriptorHandleFromIndex(0u).cpuDescriptorHandle;

		// Transition back buffer from state present to state render target.
		CD3DX12_RESOURCE_BARRIER backBufferPresentToRT = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1u, &backBufferPresentToRT);

		static const std::array<float, 4> clearColor{0.1f, 0.1f, 0.1f, 1.0f};
		commandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 1u, 0u, nullptr);

		// Set root signature, pso and render resources.
		commandList->SetGraphicsRootSignature(mRootSignature.Get());
		commandList->SetPipelineState(mPso.Get());
		struct RenderResources
		{
			uint32_t positionBufferIndex;
			uint32_t colorBufferIndex;
		};

		RenderResources renderResources =
		{
			.positionBufferIndex = mVertexPositionBuffer.srvIndex,
			.colorBufferIndex = mVertexColorBuffer.srvIndex,
		};


		const D3D12_INDEX_BUFFER_VIEW indexBufferView
		{
			.BufferLocation = mIndexBuffer.resource->GetGPUVirtualAddress(),
			.SizeInBytes = sizeof(uint32_t) * 3u,
			.Format = DXGI_FORMAT_R32_UINT,
		};

		commandList->IASetIndexBuffer(&indexBufferView);

		std::array<ID3D12DescriptorHeap* const, 1u> descriptorHeaps{ mCbvSrvUavDescriptorHeap.GetDescriptorHeap() };
		commandList->SetDescriptorHeaps(1u, { descriptorHeaps.data()});

		// Set viewport and render targets.
		commandList->RSSetViewports(1u, &mViewport);
		commandList->RSSetScissorRects(1u, &mScissorRect);

		commandList->OMSetRenderTargets(1u, &rtvHandle, FALSE, &dsvHandle);
		commandList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRoot32BitConstants(0u, 2u, &renderResources, 0u);
		commandList->DrawInstanced(3u, 1u, 0u, 0u);

		// Transition back buffer from render target to present.
		CD3DX12_RESOURCE_BARRIER backBufferRTToPresent = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1u, &backBufferRTToPresent);

		mDirectCommandQueue.ExecuteCommandLists(std::move(commandList), mCurrentBackBufferIndex);

		const uint32_t syncInterval = mVsyncEnabled ? 1u : 0u;
		const uint32_t presentFlags = mIsTearingSupported && !mVsyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0u;

		mSwapChain->Present(syncInterval, presentFlags);

		mDirectCommandQueue.Flush(mCurrentBackBufferIndex);
		mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		mFrameNumber++;
	}

	void Engine::Destroy()
	{
		mCopyCommandQueue.Flush(mCurrentBackBufferIndex);
		mDirectCommandQueue.Flush(mCurrentBackBufferIndex);
	}

	void Engine::Resize(const Uint2& clientDimensions)
	{
		if (clientDimensions != mClientDimensions)
		{
			mAspectRatio = static_cast<float>(clientDimensions.x) / static_cast<float>(clientDimensions.y);

			mClientDimensions = clientDimensions;
		}
	}

	void Engine::OnKeyAction(const uint8_t keycode, const bool isKeyDown)
	{
		if (isKeyDown)
		{
			if (keycode == VK_F5)
			{
				mVsyncEnabled = true;
			}

			if (keycode == VK_F6)
			{
				mVsyncEnabled = false;
			}
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
				ErrorMessage(L"Assets Directory not found!");
			}
		}

		const std::filesystem::path assetsDirectory = currentDirectory / "Assets";

		if (!std::filesystem::is_directory(assetsDirectory))
		{
			ErrorMessage(L"Assets Directory that was located is not a directory!");
		}

		mRootDirectoryPath = currentDirectory.wstring() + L"/";
		mAssetsDirectoryPath = currentDirectory.wstring() + L"/Assets/";
	}

	std::wstring Engine::GetAssetPath(const std::wstring_view assetPath) const
	{
		return std::move(mAssetsDirectoryPath + assetPath.data());
	}

	void Engine::LoadCoreObjects()
	{
		uint32_t dxgiCreateFactoryFlags = 0;

		// Enable the debug layer.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			ThrowIfFailed(::D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&mDebug)));
			mDebug->EnableDebugLayer();
			mDebug->SetEnableGPUBasedValidation(TRUE);
			mDebug->SetEnableSynchronizedCommandQueueValidation(TRUE);

			// Create the IDXGIFactory with CREATE_FACTORY_DEBUG in debug build.
			dxgiCreateFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}

		// Create DXGI Factory.
		ThrowIfFailed(::CreateDXGIFactory2(dxgiCreateFactoryFlags, IID_PPV_ARGS(&mFactory)));

		// Get the adapter with highest performance.
		ThrowIfFailed(mFactory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&mAdapter)));

		// Log adapter description in debug build.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			DXGI_ADAPTER_DESC3 adapterDesc{};
			ThrowIfFailed(mAdapter->GetDesc3(&adapterDesc));

			DebugLogMessage(L"Selected adapter : " + std::wstring(adapterDesc.Description));
		}

		// Check if tearing is supported.
		BOOL tearingSupported{ FALSE };
		if (SUCCEEDED(mFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported, sizeof(tearingSupported))))
		{
			tearingSupported = TRUE;
		}

		mIsTearingSupported = tearingSupported == TRUE;

		// Create D3D12 Device.
		ThrowIfFailed(::D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice)));
		SetName(mDevice.Get(), L"D3D12 Device");

		// Create info queue in debug build to set brake point on error's, warning and corruption messages.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue{};
			ThrowIfFailed(mDevice.As(&infoQueue));

			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		}

		// Create Direct command queue.
		mDirectCommandQueue.Init(mDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct Command Queue");
		mCopyCommandQueue.Init(mDevice.Get(), D3D12_COMMAND_LIST_TYPE_COPY, L"Copy Command Queue");

		// Create swapchain.
		const DXGI_SWAP_CHAIN_DESC1 swapChainDesc
		{
			.Width = mClientDimensions.x,
			.Height = mClientDimensions.y,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.Stereo = FALSE,
			.SampleDesc = {1u, 0u},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = BACK_BUFFER_COUNT,
			.Scaling = DXGI_SCALING_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
			.Flags = mIsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
		};

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain{};
		ThrowIfFailed(mFactory->CreateSwapChainForHwnd(mDirectCommandQueue.GetCommandQueue(), mWindowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
		ThrowIfFailed(swapChain.As(&mSwapChain));
		mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		// Create descriptor heaps.
		mRtvDescriptorHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BACK_BUFFER_COUNT, L"RTV Descriptor Heap");
		mDsvDescriptorHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1u, L"DSV Descriptor Heap");
		mCbvSrvUavDescriptorHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100u, L"CBV SRV UAV Descriptor Heap");

		// Create render targets.
		CreateRenderTargets();

		// Create depth stencil buffer.
		const D3D12_RESOURCE_DESC depthStencilBufferResourceDesc
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = 0u,
			.Width = mClientDimensions.x,
			.Height = mClientDimensions.y,
			.DepthOrArraySize = 1u,
			.MipLevels = 1u,
			.Format = DXGI_FORMAT_D32_FLOAT,
			.SampleDesc = {1u, 0u},
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		};

		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		const D3D12_CLEAR_VALUE optimizedClearValue
		{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil =
			{
				.Depth = 1.0f,
				.Stencil = 1u
			}
		};

		ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilBufferResourceDesc, D3D12_RESOURCE_STATE_COMMON, &optimizedClearValue, IID_PPV_ARGS(&mDepthStencilBuffer)));

		// Create DSV.
		const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc
		{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Texture2D
			{
				.MipSlice = 0u,
			}
		};

		const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle{ mDsvDescriptorHeap.GetDescriptorHandleForHeapStart().cpuDescriptorHandle};
		mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, dsvHandle);

		// Change state of DSV from common to depth write.
		const CD3DX12_RESOURCE_BARRIER dsvFromCommonToDepthWrite = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		
		auto commandList = mDirectCommandQueue.GetCommandList(mCurrentBackBufferIndex);

		commandList->ResourceBarrier(1u, &dsvFromCommonToDepthWrite);
		mDirectCommandQueue.ExecuteCommandLists(std::move(commandList), mCurrentBackBufferIndex);
		mDirectCommandQueue.Flush(mCurrentBackBufferIndex);

		// Setup viewport and scissor rect.
		mViewport =
		{
			.TopLeftX = 0.0f,
			.TopLeftY = 0.0f,
			.Width = static_cast<float>(mClientDimensions.x),
			.Height = static_cast<float>(mClientDimensions.y),
			.MinDepth = 0.0f,
			.MaxDepth = 1.0f
		};

		mScissorRect =
		{
			.left = 0l,
			.top = 0l,
			.right = static_cast<long>(mClientDimensions.x),
			.bottom = static_cast<long>(mClientDimensions.y)
		};
	}

	void Engine::LoadContentAndAssets()
	{
		const std::array<DirectX::XMFLOAT3, 3> vertexPositions
		{
			DirectX::XMFLOAT3{0.5f, -0.5f, 0.0f},
			DirectX::XMFLOAT3{-0.5f, -0.5f, 0.0f},
			DirectX::XMFLOAT3{0.0f, 0.5f, 0.0f},
		};

		const graphics::BufferCreationDesc vertexPositionsBufferCreationDesc
		{
			.usage = graphics::BufferUsage::StructuredBuffer,
			.format = DXGI_FORMAT_R32G32B32_FLOAT,
			.componentCount = 3u,
			.stride = sizeof(DirectX::XMFLOAT3)
		};

		mVertexPositionBuffer = CreateBuffer(vertexPositionsBufferCreationDesc, vertexPositions.data(), L"Vertex Positions Buffer");

		const std::array<DirectX::XMFLOAT3, 3> vertexColors
		{
			DirectX::XMFLOAT3{1.0f, 0.0f, 0.0f},
			DirectX::XMFLOAT3{0.0f, 1.0f, 0.0f},
			DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f},
		};

		const graphics::BufferCreationDesc vertexColorsBufferCreationDesc
		{
			.usage = graphics::BufferUsage::StructuredBuffer,
			.format = DXGI_FORMAT_R32G32B32_FLOAT,
			.componentCount = 3u,
			.stride = sizeof(DirectX::XMFLOAT3)
		};

		mVertexColorBuffer = CreateBuffer(vertexColorsBufferCreationDesc, vertexColors.data(), L"Vertex Colors Buffer");

		const std::array<uint32_t, 3u> indices{ 0u, 1u, 2u };

		const graphics::BufferCreationDesc indexBufferCreationDesc
		{
			.usage = graphics::BufferUsage::IndexBuffer,
			.format = DXGI_FORMAT_R32_UINT,
			.componentCount = 3u,
			.stride = sizeof(uint32_t)
		};

		mIndexBuffer = CreateBuffer(indexBufferCreationDesc, indices.data(), L"Index Buffer");

		graphics::ShaderReflection shaderReflection{};
		graphics::Shader vertexShader = graphics::ShaderCompiler::GetInstance().CompilerShader(graphics::ShaderType::Vertex, shaderReflection, GetAssetPath(L"Shaders/HelloTriangle.hlsl"));
		graphics::Shader pixelShader = graphics::ShaderCompiler::GetInstance().CompilerShader(graphics::ShaderType::Pixel, shaderReflection, GetAssetPath(L"Shaders/HelloTriangle.hlsl"));

		// Create root signature.
		const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc
		{
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1
			{
                .NumParameters = static_cast<uint32_t>(shaderReflection.rootParameters.size()),
			    .pParameters = shaderReflection.rootParameters.data(),
				.NumStaticSamplers = 0u,
			    .Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
            }
		};

		ID3DBlob* errorBlob{};
		ID3DBlob* rootSignatureBlob{};

		if (FAILED(::D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &rootSignatureBlob, &errorBlob)))
		{
			if (errorBlob)
			{
				const char* message = (char*)errorBlob->GetBufferPointer();
				ErrorMessage(message);
			}
		}

		ThrowIfFailed(mDevice->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

		// Create pipeline state object.
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc
		{
			.pRootSignature = mRootSignature.Get(),
			.VS = 
			{
				.pShaderBytecode = vertexShader.mShaderBlob->GetBufferPointer(),
				.BytecodeLength = vertexShader.mShaderBlob->GetBufferSize(),
            },
			.PS = 
			{
				.pShaderBytecode = pixelShader.mShaderBlob->GetBufferPointer(),
				.BytecodeLength = pixelShader.mShaderBlob->GetBufferSize(),
			},
			.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
			.SampleMask = UINT_MAX,
			.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
			.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1u,
			.RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
			.DSVFormat = {DXGI_FORMAT_D32_FLOAT},
			.SampleDesc = {1u, 0u}
		};

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPso)));
	}

	graphics::Buffer Engine::CreateBuffer(const graphics::BufferCreationDesc& bufferCreationDesc, const void* data, const std::wstring_view bufferName)
	{
		// Create resource and upload data to an intermediate upload buffer.
		const D3D12_RESOURCE_FLAGS resourceFlags{ D3D12_RESOURCE_FLAG_NONE };

		const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<uint64_t>(bufferCreationDesc.stride * bufferCreationDesc.componentCount), resourceFlags);

		graphics::Buffer buffer{};
		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties{D3D12_HEAP_TYPE_DEFAULT};
		ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer.resource)));

		// If buffer is non a constant buffer, create upload heap to transfer data to CPU memory -> CPU / GPU Shared memory -> GPU only memory.
		if (bufferCreationDesc.usage != graphics::BufferUsage::ConstantBuffer)
		{
			// Create upload heap and intermediate resource.
			Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource{};
			const CD3DX12_HEAP_PROPERTIES uploadHeapProperties{ D3D12_HEAP_TYPE_UPLOAD };

			ThrowIfFailed(mDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&intermediateResource)));

			// Upload data to the buffer resource.
			const LONG_PTR bufferSize = static_cast<LONG_PTR>(bufferCreationDesc.stride * bufferCreationDesc.componentCount);

			const D3D12_SUBRESOURCE_DATA subresourceData
			{
				.pData = data,
				.RowPitch = bufferSize,
				.SlicePitch = bufferSize
			};

			auto commandList = mCopyCommandQueue.GetCommandList(mCurrentBackBufferIndex);
			UpdateSubresources(commandList.Get(), buffer.resource.Get(), intermediateResource.Get(), 0u, 0u, 1u, &subresourceData);

			mCopyCommandQueue.ExecuteCommandLists(std::move(commandList), mCurrentBackBufferIndex);
			mCopyCommandQueue.Flush(mCurrentBackBufferIndex);

			if (bufferCreationDesc.usage == graphics::BufferUsage::StructuredBuffer)
			{
				// Create SRV.
				const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc
				{
					.Format = DXGI_FORMAT_UNKNOWN,
					.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
					.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					.Buffer
					{
						.FirstElement = 0u,
						.NumElements = bufferCreationDesc.componentCount,
						.StructureByteStride = static_cast<UINT>(bufferCreationDesc.stride),
						.Flags = D3D12_BUFFER_SRV_FLAG_NONE
					}
				};

				mDevice->CreateShaderResourceView(buffer.resource.Get(), &srvDesc, mCbvSrvUavDescriptorHeap.GetCurrentDescriptorHandle().cpuDescriptorHandle);
				buffer.srvIndex = mCbvSrvUavDescriptorHeap.GetCurrentDescriptorHandle().index;
			}

			mCbvSrvUavDescriptorHeap.OffsetCurrentDescriptorHandle();

			SetName(buffer.resource.Get(), bufferName);
		}

		return buffer;
	}

	void Engine::CreateRenderTargets()
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvDescriptorHeap.GetDescriptorHandleForHeapStart().cpuDescriptorHandle);

		for (uint32_t backBufferIndex : std::views::iota(0u, BACK_BUFFER_COUNT))
		{
			ThrowIfFailed(mSwapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&mSwapChainBackBuffers[backBufferIndex])));

			mDevice->CreateRenderTargetView(mSwapChainBackBuffers[backBufferIndex].Get(), nullptr, rtvHandle);
			SetName(mSwapChainBackBuffers[backBufferIndex].Get(), L"SwapChain Back Buffer");
			rtvHandle.Offset(mRtvDescriptorHeap.GetDescriptorHandleSize());
		}
	}
}