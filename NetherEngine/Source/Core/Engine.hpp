#pragma once

#include "Common/Utils.hpp"
#include "Rendering/Device.hpp"
#include "Rendering/Resources.hpp"
#include "Scene/GameObject.hpp"
#include "Scene/Camera.hpp"

namespace nether::core
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

		std::unique_ptr<rendering::Device> mDevice{};

		// Render 'sandbox' data.
		Camera mCamera{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mHelloTriangleRootSignature{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mHelloTrianglePipelineState{};

		std::unique_ptr<scene::GameObject> mCube{};
		std::unique_ptr<scene::GameObject> mSponza{};

		MVPBuffer mMvpBuffer{};
	};
}