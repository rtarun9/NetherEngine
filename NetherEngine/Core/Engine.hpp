#pragma once

#include "Graphics/DescriptorHeap.hpp"
#include "Graphics/Resources.hpp"
#include "Graphics/CommandQueue.hpp"

namespace nether::core
{
	// Central 'Engine' abstraction.
	class Engine
	{
	public:
		Engine() = default;
		~Engine();

		Engine(const Engine& other) = delete;
		Engine& operator=(const Engine& other) = delete;

		Engine(Engine&& other) = delete;
		Engine& operator=(Engine&& other) = delete;

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

		graphics::Buffer CreateBuffer(const graphics::BufferCreationDesc& bufferCreationDesc, const void* data, const std::wstring_view bufferName);

	private:
		void CreateRenderTargets();

	private:
		static const uint32_t BACK_BUFFER_COUNT = 3u;

	private:
		bool mIsInitialized{ false };

		HWND mWindowHandle{};

		Uint2 mClientDimensions{};
		float mAspectRatio{1.0f};

		uint64_t mFrameNumber{};

		std::wstring mAssetsDirectoryPath{};
		std::wstring mRootDirectoryPath{};

		bool mIsTearingSupported{false};
		bool mVsyncEnabled{true};

		// Core DirectX and DXGI objects.
		Microsoft::WRL::ComPtr<ID3D12Debug4> mDebug{};
		Microsoft::WRL::ComPtr<IDXGIFactory6> mFactory{};
		Microsoft::WRL::ComPtr<IDXGIAdapter4> mAdapter{};
		Microsoft::WRL::ComPtr<ID3D12Device5> mDevice{};

		Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain{};
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BACK_BUFFER_COUNT> mSwapChainBackBuffers{};

		graphics::DescriptorHeap mRtvDescriptorHeap{};
		graphics::DescriptorHeap mDsvDescriptorHeap{};
		graphics::DescriptorHeap mCbvSrvUavDescriptorHeap{};

		graphics::CommandQueue mDirectCommandQueue{};
		graphics::CommandQueue mCopyCommandQueue{};

		uint32_t mCurrentBackBufferIndex{};

		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer{};

		D3D12_VIEWPORT mViewport{};
		D3D12_RECT mScissorRect{};

		// Rendering sandbox data.
		graphics::Buffer mVertexPositionBuffer{};
		graphics::Buffer mVertexColorBuffer{};
		graphics::Buffer mIndexBuffer{};

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPso{};
	};
}