#pragma once

#include "Pch.hpp"

namespace nether::core
{
	// Central 'Engine' abstraction.
	class Engine
	{
	public:
		Engine();
		~Engine();

		void Init(const HWND windowHandle, const Uint2& clientDimensions);
		void Update(const float deltaTime);
		void Render();
		void Destroy();

		void Resize(const Uint2& clientDimensions);
		void OnKeyAction(const uint8_t keycode, const bool isKeyDown);

	public:

		// The assets directory is located under the root directory titled 'Assets'. Required as the executable can be located in different location's.
		void FindAssetsDirectory();

		// Returns asset directory path + assetPath.
		std::wstring GetAssetPath(const std::wstring_view assetPath) const;

		void LoadCoreObjects();
		void LoadContentAndAssets();

		void ExecuteCommandList();
		void FlushCommandQueue();

	private:
		void CreateRenderTargets();

	private:
		static const uint32_t BACK_BUFFER_COUNT = 3u;

	private:
		bool mIsInitialized{ false };

		HWND mWindowHandle{};

		Uint2 mClientDimensions{};
		float mAspectRatio{1.0f};

		std::wstring mAssetsDirectoryPath{};
		std::wstring mRootDirectoryPath{};

		bool mIsTearingSupported{false};
		bool mVsyncEnabled{true};

		// Core DirectX and DXGI objects.
		Microsoft::WRL::ComPtr<ID3D12Debug4> mDebug{};
		Microsoft::WRL::ComPtr<IDXGIFactory6> mFactory{};
		Microsoft::WRL::ComPtr<IDXGIAdapter4> mAdapter{};
		Microsoft::WRL::ComPtr<ID3D12Device5> mDevice{};
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mDirectCommandQueue{};

		Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain{};
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BACK_BUFFER_COUNT> mSwapChainBackBuffers{};

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRTVDescriptorHeap{};
		uint32_t mRTVDescriptorSize{};

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDSVDescriptorHeap{};
		uint32_t mDSVDescriptorSize{};

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCommandAllocator{};
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList3> mGraphicsCommandList{};

		uint32_t mCurrentBackBufferIndex{};

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
		std::array<uint64_t, BACK_BUFFER_COUNT> mFrameFenceValues{};

		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer{};

		D3D12_VIEWPORT mViewport{};
		D3D12_RECT mScissorRect{};
	};
}