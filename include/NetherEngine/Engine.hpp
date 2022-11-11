#pragma once

struct SDL_Window;

namespace nether
{
    class Engine
    {
      public:
        ~Engine();

        void run();

      private:
        void initPlatformBackend();
        void initGraphicsBackend();

        void initPipelines();

      public:
        static constexpr uint32_t BACK_BUFFER_COUNT = 2u;

      private:
        SDL_Window* m_window{};
        HWND m_windowHandle{};

        Uint2 m_windowDimensions{};

        Comptr<IDXGIFactory6> m_factory{};
        Comptr<IDXGIAdapter2> m_adapter{};
        Comptr<ID3D12Device5> m_device{};
        Comptr<ID3D12CommandQueue> m_directCommandQueue{};
        Comptr<ID3D12CommandAllocator> m_commandAllocator{};
        Comptr<ID3D12GraphicsCommandList1> m_commandList{};

        Comptr<ID3D12DescriptorHeap> m_rtvDescriptorHeap{};
        uint32_t m_rtvDescriptorSize{};

        std::array<Comptr<ID3D12Resource>, BACK_BUFFER_COUNT> m_backBuffers{};
        Comptr<IDXGISwapChain1> m_swapchain{};

        Pipeline m_pipeline{};
    };
}
