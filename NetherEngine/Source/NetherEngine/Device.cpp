#include "Pch.hpp"

#include "DataTypes.hpp"
#include "Utils.hpp"
#include "Shader.hpp"
#include "DescriptorHeap.hpp"
#include "Device.hpp"

namespace nether
{
	Device::Device(const HWND windowHandle, const Uint2& clientDimensions)
	{
		LoadCoreObjects(windowHandle, clientDimensions);
	}

	void Device::Resize(const Uint2& clientDimensions)
	{
		// Flush the queue to execute all pending operations, reset swapchain back buffers, resize the buffer's, and create the RTV's again.
		mDirectCommandQueue->Flush(mCurrentSwapChainBackBufferIndex);

		for (const uint32_t bufferIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			mSwapChainBackBuffers[bufferIndex].Reset();
			mDirectCommandQueue->mFrameFenceValues[bufferIndex] = mDirectCommandQueue->mFrameFenceValues[mCurrentSwapChainBackBufferIndex];
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		utils::ThrowIfFailed(mSwapChain->GetDesc(&swapChainDesc));
		utils::ThrowIfFailed(mSwapChain->ResizeBuffers(NUMBER_OF_FRAMES, clientDimensions.x, clientDimensions.y, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		mCurrentSwapChainBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		CreateRenderTargetViews(clientDimensions);
		CreateDepthStencilBuffer(clientDimensions);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCurrentBackBufferRtvHandle() const
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(mRtvDescriptorHeap->GetDescriptorHandleFromHeapStart().cpuDescriptorHandle, mCurrentSwapChainBackBufferIndex, mRtvDescriptorHeap->GetDescriptorHandleSize());
		return rtvDescriptorHandle;
	}

	void Device::StartFrame()
	{
		utils::ThrowIfFailed(mDirectCommandQueue->mCommandAllocators[mCurrentSwapChainBackBufferIndex]->Reset());
	}

	void Device::Present()
	{
		const UINT syncInterval = mVsync ? 1u : 0u;
		const UINT presentFlags = !mVsync && mTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0u;

		mSwapChain->Present(syncInterval, presentFlags);
	}

	void Device::EndFrame()
	{
		mCurrentSwapChainBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		mDirectCommandQueue->WaitForFenceValue(mDirectCommandQueue->mFrameFenceValues[mCurrentSwapChainBackBufferIndex]);
	}

	void Device::SetVsync(const bool vsync)
	{
		mVsync = vsync;
	}

	void Device::LoadCoreObjects(const HWND windowHandle, const Uint2& clientDimensions)
	{
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

		// Create queue's.
		mDirectCommandQueue = std::make_unique<CommandQueue>(mDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct Command Queue");

		// Check for tearing support (must be supported by both the monitor and the adapter).
		BOOL tearingSupported{ FALSE };
		if (SUCCEEDED(mFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported, sizeof(tearingSupported))))
		{
			mTearingSupported = true;
		}

		// Create the swapchain (implements surfaces for rendering data before presenting it to a output).
		const DXGI_SWAP_CHAIN_DESC1 swapChainDesc
		{
			.Width = clientDimensions.x,
			.Height = clientDimensions.y,
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
		utils::ThrowIfFailed(mFactory->CreateSwapChainForHwnd(mDirectCommandQueue->mCommandQueue.Get(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
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

		// Create descriptor heaps.
		mRtvDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 5u, L"RTV Descriptor Heap");
		mDsvDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 5u, L"DSV Descriptor Heap");
		mCbvSrvUavDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5u, L"CBV SRV UAV Descriptor Heap");
		mSamplerDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 5u, L"Sampler Descriptor Heap");

		CreateRenderTargetViews(clientDimensions);
		CreateDepthStencilBuffer(clientDimensions);
	}

	void Device::CreateRenderTargetViews(const Uint2& clientDimensions)
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

		mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)clientDimensions.x, (float)clientDimensions.y);
	}

	void Device::CreateDepthStencilBuffer(const Uint2& clientDimensions)
	{
		// Create DSV buffer.
		const D3D12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		const CD3DX12_RESOURCE_DESC depthStencilResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, clientDimensions.x, clientDimensions.y, 1u, 0u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
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

	VertexBuffer Device::CreateVertexBuffer(const VertexBufferCreationDesc& vertexBufferCreationDesc, void* data, const std::wstring_view bufferName)
	{
		if (!data)
		{
			utils::ErrorMessage(L"Cannot create vertex buffer with no data.");
		}
		
		const size_t bufferSize = static_cast<size_t>(vertexBufferCreationDesc.numberOfElements * vertexBufferCreationDesc.stride);

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

		auto commandList = mDirectCommandQueue->GetCommandList(mDevice.Get(), mCurrentSwapChainBackBufferIndex);
		UpdateSubresources(commandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0u, 0u, 1u, &subresourceData);
		mDirectCommandQueue->ExecuteCommandLists(commandList, mCurrentSwapChainBackBufferIndex);
		mDirectCommandQueue->Flush(mCurrentSwapChainBackBufferIndex);

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
		return vertexBuffer;
	}

	IndexBuffer Device::CreateIndexBuffer(const IndexBufferCreationDesc& indexBufferCreationDesc, void* data, const std::wstring_view bufferName)
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

		auto commandList = mDirectCommandQueue->GetCommandList(mDevice.Get(), mCurrentSwapChainBackBufferIndex);
		UpdateSubresources(commandList.Get(), destinationResource.Get(), intermediateResource.Get(), 0u, 0u, 1u, &subresourceData);
		mDirectCommandQueue->ExecuteCommandLists(commandList, mCurrentSwapChainBackBufferIndex);
		mDirectCommandQueue->Flush(mCurrentSwapChainBackBufferIndex);

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
		return indexBuffer;
	}

	ConstantBuffer Device::CreateConstantBuffer(const ConstantBufferCreationDesc& constantBufferCreationDesc, const std::wstring_view bufferName)
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
		return constantBuffer;
	}
}