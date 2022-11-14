#pragma once
#pragma once

struct SDL_Window;

namespace nether
{
    struct alignas(256) TransformData
    {
        math::XMMATRIX mvpMatrix{};
    };

    class Engine
    {
      public:
        ~Engine();
        void run();

      private:
        void update(const float deltaTime);
        void render();

        void flushGPU();

      private:
        FrameResources& getCurrentFrameResources() { return m_frameResources[m_frameIndex]; }

        void initPlatformBackend();
        
        void initGraphicsBackend();

        void initDescriptorHeaps();
        void initCommandObjects();
        void initSyncPrimitives();
        void initSwapchain();

        void initPipelines();
        void initMeshes();

        void uploadBuffers();

      private:
        // If data is nullptr, a buffer with CPU / GPU access will be created. Else, a GPU only buffer will be created.
        [[nodiscard]] Comptr<ID3D12Resource> createBuffer(const D3D12_RESOURCE_DESC& bufferResourceDesc,
                                                          const std::byte* data, const std::wstring_view bufferName);

      public:
        static constexpr uint32_t FRAME_COUNT = 2u;

      private:
        SDL_Window* m_window{};
        HWND m_windowHandle{};

        Uint2 m_windowDimensions{};

        D3D12_VIEWPORT m_viewport{};
        D3D12_RECT m_scissorRect{};

        Comptr<IDXGIFactory6> m_factory{};
        Comptr<IDXGIAdapter2> m_adapter{};
        Comptr<ID3D12Device5> m_device{};

        Comptr<ID3D12CommandQueue> m_directCommandQueue{};
        std::array<FrameResources, FRAME_COUNT> m_frameResources{};

        Comptr<ID3D12CommandQueue> m_copyCommandQueue{};
        Comptr<ID3D12Fence> m_copyFence{};

        Comptr<ID3D12CommandAllocator> m_copyCommandAllocator{};
        Comptr<ID3D12GraphicsCommandList2> m_copyCommandList{};

        std::vector<Comptr<ID3D12Resource>> m_uploadBuffers{};

        uint32_t m_frameIndex{};
        uint32_t m_frameCount{};

        Comptr<ID3D12Fence1> m_fence{};
        HANDLE m_fenceEvent{};
        
        Comptr<ID3D12DescriptorHeap> m_rtvDescriptorHeap{};
        uint32_t m_rtvDescriptorSize{};

        Comptr<ID3D12DescriptorHeap> m_dsvDescriptorHeap{};
        uint32_t m_dsvDescriptorSize{};

        std::array<Comptr<ID3D12Resource>, FRAME_COUNT> m_backBuffers{};
        Comptr<IDXGISwapChain3> m_swapchain{};

        Comptr<ID3D12Resource> m_depthStencilTexture{};

        Pipeline m_pipeline{};
        Mesh m_triangleMesh{};

        ConstantBuffer<TransformData> m_transformBuffer {};
    };
}
