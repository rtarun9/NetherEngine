#pragma once

#include "Device.hpp"
#include "Utils.hpp"
#include "Model.hpp"
#include "Camera.hpp"

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

	public:
		void FindAssetsDirectory();
		std::wstring GetAssetPath(const std::wstring_view assetPath) const;

		void LoadContentAndAssets();

	private:
		bool mIsInitialized{ false };

		Uint2 mClientDimensions{};
		float mAspectRatio{ 0.0f };

		std::wstring mAssetsDirectoryPath{};

		std::unique_ptr<Device> mDevice{};

		// Render 'sandbox' data.
		Camera mCamera{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mHelloTriangleRootSignature{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mHelloTrianglePipelineState{};

		std::unique_ptr<Model> mCube{};
		std::unique_ptr<Model> mSponza{};

		std::unique_ptr<ConstantBuffer> mConstantBuffer{};
		std::unique_ptr<ConstantBuffer> mSponzaConstantBuffer{};
		MVPBuffer mMvpBuffer{};
	};
}