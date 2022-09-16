#pragma once

#include "DataTypes.hpp"

namespace nether
{
	class Engine
	{
	public:
		Engine() = default;
		~Engine();

		void Init(const HWND windowHandle, const Uint2& clientDimensions);
		void Update(float deltaTime);
		void Render();
		void Destroy();

		void Resize(const Uint2& clientDimensions);
		void OnKeyAction(const uint8_t keycode, const bool isKeyDown);

	private:
		void StartFrame();
		void Present();
		void EndFrame();

		uint64_t Signal();
		void WaitForFenceValue(const uint64_t fenceValue);
		void Flush();

	private:
		void CreateRenderTargetViews();

	private:
		static constexpr uint32_t NUMBER_OF_FRAMES = 3u;
		static constexpr DXGI_FORMAT SWAP_CHAIN_BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

	private:
		bool mIsInitialized{false};
		bool mVsync{true};
		bool mTearingSupported{false};

		Uint2 mClientDimensions{};

		// DirectX12 and DXGI objects.
		Microsoft::WRL::ComPtr<ID3D12Debug4> mDebugController{};
		Microsoft::WRL::ComPtr<IDXGIFactory6> mFactory{};
		Microsoft::WRL::ComPtr<IDXGIAdapter3> mAdapter{};
		Microsoft::WRL::ComPtr<ID3D12Device5> mDevice{};
		
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mDirectCommandQueue{};
		
		Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain{};
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, NUMBER_OF_FRAMES> mSwapChainBackBuffers{};
		uint32_t mCurrentSwapChainBackBufferIndex{};

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvDescriptorHeap{};
		uint32_t mRtvDescriptorSize{};

		std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, NUMBER_OF_FRAMES> mCommandAllocators{};
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> mCommandList{};

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
		HANDLE mFenceEvent{};
		uint64_t mFenceValue{};
		std::array<uint64_t, NUMBER_OF_FRAMES> mFrameFenceValues{};
	};
}