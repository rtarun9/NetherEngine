#pragma once

namespace nether::core
{
	// Forward declaration's.
	class Engine;
	
	// Holds / manages the Win32 window, and control's the main game / message loop.
	// Calls the Update / Render / KeyEvent and other Engine function's. 
	class Application
	{
	public:
		int Run(Engine* const engine, const std::wstring_view windowTitle);

		Application() = default;
		~Application() = default;
		
		Application(const Application& other) = delete;
		Application& operator=(const Application& other) = delete;
		
		Application(Application&& other) = delete;
		Application& operator=(Application&& other) = delete;

	private:
		static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	private:
		// Window handle.
		HWND mWindowHandle{};

		// Timing related variables for calculation of delta time.
		std::chrono::high_resolution_clock mClock{};
		std::chrono::high_resolution_clock::time_point mCurrentFrameTime{};
		std::chrono::high_resolution_clock::time_point mPreviousFrameTime{};
	};
}