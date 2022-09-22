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

		HWND mWindowHandle{};

		Uint2 mClientDimensions{};
		float mAspectRatio{ 0.0f };

		std::wstring mAssetsDirectoryPath{};
		std::wstring mRootDirectoryPath{};

		std::unique_ptr<rendering::Device> mDevice{};

		// Render 'sandbox' data.
		Camera mCamera{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mHelloTriangleRootSignature{};
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mHelloTrianglePipelineState{};

		std::unordered_map<std::wstring, std::unique_ptr<scene::GameObject>> mGameObjects{};

		struct alignas(256) SceneConstantBufferData
		{
			// Light is initially looking down (i.e) from the light source.
			DirectX::XMFLOAT3 directionalLightDirection{ 0.0f, -1.0f, 0.0f };
			float padding{};
			DirectX::XMFLOAT3 directionalLightColor{ 1.0f, 1.0f, 1.0f };
			float padding2{};
			DirectX::XMFLOAT3 cameraPosition{};
			float padding3{};
		};

		SceneConstantBufferData mSceneConstantBufferData{};
		std::unique_ptr<rendering::ConstantBuffer> mSceneConstantBuffer{};

		rendering::ShaderReflection mShaderReflection{};
	
	};
}