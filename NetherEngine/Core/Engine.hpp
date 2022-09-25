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
	};
}