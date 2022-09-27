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
		ThrowIfFailed(mDirectCommandAllocator->Reset());
		ThrowIfFailed(mGraphicsCommandList->Reset(mDirectCommandAllocator.Get(), nullptr));

		// Clear RTV and DSV.
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvDescriptorHeap.GetDescriptorHandleFromIndex(mCurrentBackBufferIndex).cpuDescriptorHandle;
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDsvDescriptorHeap.GetDescriptorHandleFromIndex(0u).cpuDescriptorHandle;

		// Transition back buffer from state present to state render target.
		CD3DX12_RESOURCE_BARRIER backBufferPresentToRT = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGraphicsCommandList->ResourceBarrier(1u, &backBufferPresentToRT);

		static const std::array<float, 4> clearColor{0.1f, 0.1f, 0.1f, 1.0f};
		mGraphicsCommandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);
		mGraphicsCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 1u, 0u, nullptr);

		// Set viewport and render targets.
		mGraphicsCommandList->RSSetViewports(1u, &mViewport);
		mGraphicsCommandList->RSSetScissorRects(1u, &mScissorRect);
		mGraphicsCommandList->OMSetRenderTargets(1u, &rtvHandle, FALSE, &dsvHandle);

		// Transition back buffer from render target to present.
		CD3DX12_RESOURCE_BARRIER backBufferRTToPresent = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mGraphicsCommandList->ResourceBarrier(1u, &backBufferRTToPresent);

		ThrowIfFailed(mGraphicsCommandList->Close());
		ExecuteCommandList();

		const uint32_t syncInterval = mVsyncEnabled ? 1u : 0u;
		const uint32_t presentFlags = mIsTearingSupported && !mVsyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0u;

		mSwapChain->Present(syncInterval, presentFlags);

		FlushCommandQueue();

		mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();
		mFrameNumber++;
	}

	void Engine::Destroy()
	{
		FlushCommandQueue();
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
		const D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0u
		};

		ThrowIfFailed(mDevice->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(&mDirectCommandQueue)));
		SetName(mDirectCommandQueue.Get(), L"Direct Command Queue");

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
		ThrowIfFailed(mFactory->CreateSwapChainForHwnd(mDirectCommandQueue.Get(), mWindowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
		ThrowIfFailed(swapChain.As(&mSwapChain));
		mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		// Create descriptor heaps.
		mRtvDescriptorHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BACK_BUFFER_COUNT, L"RTV Descriptor Heap");
		mDsvDescriptorHeap.Init(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1u, L"DSV Descriptor Heap");

		// Create render targets.
		CreateRenderTargets();

		// Create command allocator and command list.
		ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCommandAllocator)));
		SetName(mDirectCommandAllocator.Get(), L"Direct Command Allocator");

		ThrowIfFailed(mDevice->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&mGraphicsCommandList)));
		SetName(mGraphicsCommandList.Get(), L"Graphics Command List");
		ThrowIfFailed(mGraphicsCommandList->Close());

		// Create fence object.
		ThrowIfFailed(mDevice->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		SetName(mFence.Get(), L"Fence");

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

		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_CLEAR_VALUE optimizedClearValue
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

		ThrowIfFailed(mGraphicsCommandList->Reset(mDirectCommandAllocator.Get(), nullptr));

		// Change state of DSV from common to depth write.
		CD3DX12_RESOURCE_BARRIER dsvFromCommonToDepthWrite = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mGraphicsCommandList->ResourceBarrier(1u, &dsvFromCommonToDepthWrite);

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

		ThrowIfFailed(mGraphicsCommandList->Close());
		ExecuteCommandList();
		FlushCommandQueue();
	}

	void Engine::LoadContentAndAssets()
	{
		graphics::ShaderReflection shaderReflection{};
		graphics::Shader vertexShader = graphics::ShaderCompiler::GetInstance().CompilerShader(graphics::ShaderType::Vertex, shaderReflection, GetAssetPath(L"Shaders/HelloTriangle.hlsl"));
		graphics::Shader pixelShader = graphics::ShaderCompiler::GetInstance().CompilerShader(graphics::ShaderType::Pixel, shaderReflection, GetAssetPath(L"Shaders/HelloTriangle.hlsl"));
	}

	void Engine::ExecuteCommandList()
	{
		const std::array<ID3D12CommandList* const, 1u> commandLists
		{
			mGraphicsCommandList.Get()
		};

		mDirectCommandQueue->ExecuteCommandLists(1u, commandLists.data());
	}

	void Engine::FlushCommandQueue()
	{
		mFrameFenceValues[mCurrentBackBufferIndex]++;

		ThrowIfFailed(mDirectCommandQueue->Signal(mFence.Get(), mFrameFenceValues[mCurrentBackBufferIndex]));

		if (mFence->GetCompletedValue() < mFrameFenceValues[mCurrentBackBufferIndex])
		{
			// Create HANDLE that will be used to block CPU thread until GPU has completed execution.
			HANDLE fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (!fenceEvent)
			{
				ErrorMessage(L"Failed to create fence event");
			}

			// Specify that the fence event will be fired when the fence reaches the specified value.
			ThrowIfFailed(mFence->SetEventOnCompletion(mFrameFenceValues[mCurrentBackBufferIndex], fenceEvent));
			
			// Wait until that event has been triggered.

			assert(fenceEvent && "Fence Event is NULL");
			::WaitForSingleObject(fenceEvent, INFINITE);
			CloseHandle(fenceEvent);
		}
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