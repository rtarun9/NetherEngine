#pragma once

#include "DescriptorHeap.hpp"
#include "Shader.hpp"
#include "DataTypes.hpp"
#include "Resources.hpp"
#include "Common.hpp"
#include "CommandQueue.hpp"

namespace nether
{
	class Device
	{
	public:
		Device(const HWND windowHandle, const Uint2& clientDimensions);
		~Device() = default;

		void Resize(const Uint2& clientDimensions);

		// Getters
		ID3D12Resource* GetCurrentBackBufferResource() const { return mSwapChainBackBuffers[mCurrentSwapChainBackBufferIndex].Get(); }
		DescriptorHeap* GetRtvDescriptorHeap() const { return mRtvDescriptorHeap.get(); }
		ID3D12Device5* GetDevice() const { return mDevice.Get(); }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRtvHandle() const;

	// Renderer related function's.
	public:
		void StartFrame();
		void Present();
		void EndFrame();

		void SetVsync(const bool vsync);

	public:
		// Creation functions.
		void LoadCoreObjects(const HWND windowHandle, const Uint2& clientDimensions);

		void CreateRenderTargetViews(const Uint2& clientDimensions);
		void CreateDepthStencilBuffer(const Uint2& clientDimensions);

		VertexBuffer CreateVertexBuffer(const VertexBufferCreationDesc& vertexBufferCreationDesc, void* data, const std::wstring_view bufferName);
		IndexBuffer CreateIndexBuffer(const IndexBufferCreationDesc& indexBufferCreationDesc, void* data, const std::wstring_view bufferName);
		ConstantBuffer CreateConstantBuffer(const ConstantBufferCreationDesc& constantBufferCreationDesc, const std::wstring_view bufferName);

	public:
		bool mVsync{ true };
		bool mTearingSupported{ false };

		// DirectX12 and DXGI objects.
		Microsoft::WRL::ComPtr<ID3D12Debug5> mDebugController{};
		Microsoft::WRL::ComPtr<IDXGIFactory6> mFactory{};
		Microsoft::WRL::ComPtr<IDXGIAdapter3> mAdapter{};
		Microsoft::WRL::ComPtr<ID3D12Device5> mDevice{};

		Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain{};
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, NUMBER_OF_FRAMES> mSwapChainBackBuffers{};
		uint32_t mCurrentSwapChainBackBufferIndex{};

		std::unique_ptr<DescriptorHeap> mRtvDescriptorHeap{};
		std::unique_ptr<DescriptorHeap> mDsvDescriptorHeap{};
		std::unique_ptr<DescriptorHeap> mCbvSrvUavDescriptorHeap{};
		std::unique_ptr<DescriptorHeap> mSamplerDescriptorHeap{};

		std::unique_ptr<CommandQueue> mDirectCommandQueue{};

		const CD3DX12_RECT mScissorRect{ 0, 0, LONG_MAX, LONG_MAX };
		CD3DX12_VIEWPORT mViewport{};

		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer{};
	};
}