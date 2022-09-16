#pragma once

#include "DataTypes.hpp"

namespace nether
{
	class Engine;

	class Application
	{
	public:
		Application() = default;

		int Run(Engine* engine, const HINSTANCE instance, const std::wstring_view windowTitle);

	private:
		static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	private:
		HWND mWindowHandle{};

		std::chrono::high_resolution_clock mClock{};
		std::chrono::high_resolution_clock::time_point mCurrentFrameTime{};
		std::chrono::high_resolution_clock::time_point mPreviousFrameTime{};
	};
}