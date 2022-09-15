#include "Pch.hpp"

LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            ::DestroyWindow(windowHandle);
        }
    }break;

    case WM_DESTROY:
    {
        ::PostQuitMessage(0);
        return 0;
    }break;
    }

    return ::DefWindowProc(windowHandle, message, wParam, lParam);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR cmdLine, _In_ int cmdShow)
{
    // Register the window class.

    // Specify that it should redraw the entire window if the client region dimension's change due to movement or size adjustment.
    const WNDCLASSEXW windowClass
    {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = ::WindowProc,
        .hInstance = hInstance,
        .lpszClassName = L"Base Window Class",
    };

    ::RegisterClassExW(&windowClass);
    
    // Enable DPI awareness.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Find window dimension's so that the client region covers 80% of the monitor.
    const int32_t monitorWidth = ::GetSystemMetrics(SM_CXSCREEN);
    const int32_t monitorHeight = ::GetSystemMetrics(SM_CYSCREEN);

    const int32_t clientWidth = static_cast<int32_t>(monitorWidth * 0.8f);
    const int32_t clientHeight = static_cast<int32_t>(monitorHeight * 0.8f);

    RECT windowRect
    {
        .left = 0l,
        .top = 0l,
        .right = static_cast<long>(clientWidth),
        .bottom = static_cast<long>(clientHeight)
    };

    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    const int32_t windowWidth = static_cast<int32_t>(windowRect.right - windowRect.left);
    const int32_t windowHeight = static_cast<int32_t>(windowRect.bottom - windowRect.top);

    const int32_t windowXPos = std::max<int32_t>(0, (monitorWidth - windowWidth) / 2);
    const int32_t windowYPos = std::max<int32_t>(0, (monitorHeight - windowHeight) / 2);

    // Create the window.
    const HWND windowHandle = ::CreateWindowExW(0, L"Base Window Class", L"Crystal Engine",
        WS_OVERLAPPEDWINDOW, windowXPos, windowYPos, windowWidth, windowHeight, nullptr, nullptr, hInstance, nullptr);

    if (!windowHandle)
    {
        return -1;
    }

    ::ShowWindow(windowHandle, SW_SHOWNORMAL);

    // Message loop.
    MSG message{};
    while (message.message != WM_QUIT)
    {
        if (::PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    return static_cast<int32_t>(message.wParam);
}