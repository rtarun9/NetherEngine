#pragma once

#include "DataTypes.hpp"
#include "Resources.hpp"
#include "DescriptorHeap.hpp"

#include "Utils.hpp"
#include "Model.hpp"

struct alignas(256) MVPBuffer
{
	DirectX::XMMATRIX modelMatrix{};
	DirectX::XMMATRIX viewProjectionMatrix{};
};

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

	// Renderer related function's.
	private:
		void StartFrame();
		void Present();
		void EndFrame();

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
		void ExecuteCommandList();

		uint64_t Signal();
		void WaitForFenceValue(const uint64_t fenceValue);
		void Flush();

	public:
		void FindAssetsDirectory();
		std::wstring GetAssetPath(const std::wstring_view assetPath) const;

		void LoadCoreObjects(const HWND windowHandle, const Uint2& clientDimensions);
		void LoadContentAndAssets();

	public:
		// Creation functions.
		void CreateRenderTargetViews();
		void CreateDepthStencilBuffer();

		std::unique_ptr<VertexBuffer> CreateVertexBuffer(const VertexBufferCreationDesc& vertexBufferCreationDesc, void* data, const std::wstring_view bufferName);
		std::unique_ptr<IndexBuffer> CreateIndexBuffer(const IndexBufferCreationDesc& indexBufferCreationDesc, void* data, const std::wstring_view bufferName);
		std::unique_ptr<ConstantBuffer> CreateConstantBuffer(const ConstantBufferCreationDesc& constantBufferCreationDesc, const std::wstring_view bufferName);

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

		std::unique_ptr<DescriptorHeap> mRtvDescriptorHeap{};
		std::unique_ptr<DescriptorHeap> mDsvDescriptorHeap{};
		std::unique_ptr<DescriptorHeap> mCbvSrvUavDescriptorHeap{};
		std::unique_ptr<DescriptorHeap> mSamplerDescriptorHeap{};

		std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, NUMBER_OF_FRAMES> mCommandAllocators{};
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> mCommandList{};

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
		HANDLE mFenceEvent{};
		uint64_t mFenceValue{};
		std::array<uint64_t, NUMBER_OF_FRAMES> mFrameFenceValues{};

		const CD3DX12_RECT mScissorRect{ 0, 0, LONG_MAX, LONG_MAX };
		CD3DX12_VIEWPORT mViewport{};

		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer{};

		std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> mIntermediateResources{};

		// Render 'sandbox' data.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mHelloTriangleRootSignature{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mHelloTrianglePipelineState{};

		std::unique_ptr<Model> mCube{};
		std::unique_ptr<ConstantBuffer> mConstantBuffer{};
		MVPBuffer mMvpBuffer{};
	};
}