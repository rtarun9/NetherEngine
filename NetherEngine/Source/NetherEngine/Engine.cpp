#include "Pch.hpp"

#include "Engine.hpp"

#include "DataTypes.hpp"
#include "Utils.hpp"

namespace nether
{
	Engine::~Engine()
	{
		Destroy();
	}
	
	void Engine::Init(const HWND windowHandle, const Uint2& clientDimensions)
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

			dxgiFactoryCreateFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}

		// Create DXGI factory so adapters can be queried.
		utils::ThrowIfFailed(::CreateDXGIFactory2(dxgiFactoryCreateFlags, IID_PPV_ARGS(&mFactory)));

		// Get the GPU adapter with best performance.
		utils::ThrowIfFailed(mFactory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&mAdapter)));

		// Log the adapter name if in debug mode.
		if constexpr (NETHER_DEBUG_BUILD)
		{
			DXGI_ADAPTER_DESC adapterDesc{};
			utils::ThrowIfFailed(mAdapter->GetDesc(&adapterDesc));
			::OutputDebugStringW(adapterDesc.Description);
		}

		// Create D3D12 device.
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

		// Create direct command queue.
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
		BOOL tearingSupported{FALSE};
		if (SUCCEEDED(mFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported, sizeof(tearingSupported))))
		{
			mTearingSupported = true;
		}

		// Create the swapchain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc
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
		
		// Create descriptor heaps.
		D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc
		{
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 10u,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0u,
		};

		utils::ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&mRtvDescriptorHeap)));
		utils::SetName(mRtvDescriptorHeap.Get(), L"RTV Descriptor Heap");

		mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		
		CreateRenderTargetViews();

		// Create command allocators.
		for (const uint32_t frameIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[frameIndex]));
			utils::SetName(mCommandAllocators[frameIndex].Get(), L"Command Allocator");
		}


		// Create command list.
		mDevice->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[mCurrentSwapChainBackBufferIndex].Get(), nullptr, IID_PPV_ARGS(&mCommandList));
		utils::ThrowIfFailed(mCommandList->Close());

		// Create synchronization primitives.
		utils::ThrowIfFailed(mDevice->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		utils::SetName(mFence.Get(), L"Direct Command Queue Fence"); 

		mFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!mFenceEvent)
		{
			utils::ErrorMessage(L"Failed to create fence event.");
		}

		mIsInitialized = true;
	}

	void Engine::Update(float deltaTime)
	{
	}

	void Engine::Render()
	{
		StartFrame();

		CD3DX12_RESOURCE_BARRIER backBufferPresentToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentSwapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1u, &backBufferPresentToRenderTarget);

		static constexpr std::array<float, 4> clearColor{ 0.2f, 0.2f, 0.2f, 1.0f };
		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mCurrentSwapChainBackBufferIndex, mRtvDescriptorSize);

		mCommandList->OMSetRenderTargets(1u, &rtvHandle, FALSE, nullptr);
		mCommandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);

		CD3DX12_RESOURCE_BARRIER backBufferRenderTargetToPresent = CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBackBuffers[mCurrentSwapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1u, &backBufferRenderTargetToPresent);

		utils::ThrowIfFailed(mCommandList->Close());
		std::array<ID3D12CommandList* const, 1> commandLists
		{
			mCommandList.Get()
		};

		mDirectCommandQueue->ExecuteCommandLists(1u, commandLists.data());

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
			mClientDimensions = clientDimensions;

			// Don't allow swapchain back buffers to have width / height as 0u.
			mClientDimensions.x = std::max<uint32_t>(mClientDimensions.x, 1u);
			mClientDimensions.y = std::max<uint32_t>(mClientDimensions.y, 1u);

			// Flush the queue to execute all pending operations, reset swapchain back buffers, resize the buffer's, and create the RTV's again.
			Flush();

			for (const uint32_t bufferIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
			{
				mSwapChainBackBuffers[bufferIndex].Reset();
				mFrameFenceValues[bufferIndex] = mFrameFenceValues[mCurrentSwapChainBackBufferIndex];
			}

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
			utils::ThrowIfFailed(mSwapChain->GetDesc1(&swapChainDesc));
			utils::ThrowIfFailed(mSwapChain->ResizeBuffers(NUMBER_OF_FRAMES, mClientDimensions.x, mClientDimensions.y, swapChainDesc.Format, swapChainDesc.Flags));

			mCurrentSwapChainBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

			CreateRenderTargetViews();
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
			::WaitForSingleObject(mFenceEvent, DWORD_MAX);
		}
	}

	void Engine::Flush()
	{
		const uint64_t fenceValue = Signal();
		WaitForFenceValue(fenceValue);
		mFrameFenceValues[mCurrentSwapChainBackBufferIndex] = fenceValue;
	}

	void Engine::CreateRenderTargetViews()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuDescriptorHandle = mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		
		// Create RTV's for the swapchain back buffers.
		for (const uint32_t bufferIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			mSwapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(mSwapChainBackBuffers[bufferIndex].ReleaseAndGetAddressOf()));
			mDevice->CreateRenderTargetView(mSwapChainBackBuffers[bufferIndex].Get(), nullptr, rtvCpuDescriptorHandle);

			// Offset descriptor handle.
			rtvCpuDescriptorHandle.ptr += mRtvDescriptorSize;
		}
	}
}