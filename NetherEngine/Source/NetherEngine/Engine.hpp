#pragma once

#include "DataTypes.hpp"

// Forward declarations.
namespace D3D12MA
{
	struct Allocator;
}

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
		void FindAssetsDirectory();
		std::wstring GetAssetPath(const std::wstring_view assetPath) const;

		void LoadCoreObjects(const HWND windowHandle, const Uint2& clientDimensions);
		void LoadContentAndAssets();

		void CreateRenderTargetViews();

	private:
		static constexpr uint32_t NUMBER_OF_FRAMES = 3u;
		static constexpr DXGI_FORMAT SWAP_CHAIN_BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

	private:
		bool mIsInitialized{false};
		bool mVsync{true};
		bool mTearingSupported{false};

		Uint2 mClientDimensions{};
		float mAspectRatio{0.0f};

		std::wstring mAssetsDirectoryPath{};

		// DirectX12 and DXGI objects.
		Microsoft::WRL::ComPtr<ID3D12Debug5> mDebugController{};
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

		// D3D12MA core allocator.
		D3D12MA::Allocator *mAllocator{};

		const CD3DX12_RECT mScissorRect{ 0, 0, LONG_MAX, LONG_MAX };
		CD3DX12_VIEWPORT mViewport{};

		// Render 'sandbox' data.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mHelloTriangleRootSignature{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mHelloTrianglePipelineState{};

		Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer{};
		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView{};

		Microsoft::WRL::ComPtr<ID3D12Resource> mConstantBuffer{};
		UINT8* mConstantBufferPointer{};
	};
}