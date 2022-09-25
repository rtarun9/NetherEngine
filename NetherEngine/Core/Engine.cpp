#include "Pch.hpp"

#include "Engine.hpp"

namespace nether::core
{
	Engine::Engine()
	{
		FindAssetsDirectory();
	}

	Engine::~Engine()
	{
		Destroy();
	}
	
	void Engine::Init(const HWND windowHandle, const Uint2& clientDimensions)
	{
		mWindowHandle = windowHandle;

		mClientDimensions = clientDimensions;
	
		LoadContentAndAssets();

		mIsInitialized = true;
	}

	void Engine::Update(const float deltaTime)
	{
	}

	void Engine::Render()
	{
	
	}

	void Engine::Destroy()
	{
	}

	void Engine::Resize(const Uint2& clientDimensions)
	{
		if (clientDimensions != mClientDimensions)
		{
			mAspectRatio = static_cast<float>(clientDimensions.x) / static_cast<float>(clientDimensions.y);

			mClientDimensions = clientDimensions;
		}
	}

	void Engine::OnKeyAction(const uint8_t keycode, const bool isKeyDown)
	{
	}

	void Engine::FindAssetsDirectory()
	{
		std::filesystem::path currentDirectory = std::filesystem::current_path();

		while (!std::filesystem::exists(currentDirectory / "Assets"))
		{
			if (currentDirectory.has_parent_path())
			{
				currentDirectory = currentDirectory.parent_path();
			}
			else
			{
				ErrorMessage(L"Assets Directory not found!");
			}
		}

		const std::filesystem::path assetsDirectory = currentDirectory / "Assets";

		if (!std::filesystem::is_directory(assetsDirectory))
		{
			ErrorMessage(L"Assets Directory that was located is not a directory!");
		}

		mRootDirectoryPath = currentDirectory.wstring() + L"/";
		mAssetsDirectoryPath = currentDirectory.wstring() + L"/Assets/";
	}

	std::wstring Engine::GetAssetPath(const std::wstring_view assetPath) const
	{
		return std::move(mAssetsDirectoryPath + assetPath.data());
	}

	void Engine::LoadContentAndAssets()
	{
	}
}