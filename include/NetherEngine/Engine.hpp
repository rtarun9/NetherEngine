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

        // Helper functions for creating specific types of buffers (vertex, index, constant etc). Might be removed eventually, or made into templated functions.
        [[nodiscard]] VertexBuffer createVertexBuffer(const std::byte* data, const uint32_t bufferSize, const std::wstring_view vertexBufferName);
        [[nodiscard]] IndexBuffer createIndexBuffer(const std::byte* data, const uint32_t bufferSize, const std::wstring_view indexBufferName);
        template <typename T> [[nodiscard]] ConstantBuffer<T> createConstantBuffer(const std::wstring_view constantBufferName);

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

        Comptr<ID3D12DescriptorHeap> m_cbvSrvUavDescriptorHeap{};
        uint32_t m_cbvSrvUavDescriptorSize{};
        CD3DX12_CPU_DESCRIPTOR_HANDLE m_currentCbvSrvUavDescriptorHeapHandle{};

        std::array<Comptr<ID3D12Resource>, FRAME_COUNT> m_backBuffers{};
        Comptr<IDXGISwapChain3> m_swapchain{};

        Comptr<ID3D12Resource> m_depthStencilTexture{};

        Pipeline m_pipeline{};
        Mesh m_triangleMesh{};

        ConstantBuffer<TransformData> m_transformBuffer {};
    };

    template <typename T> inline ConstantBuffer<T> Engine::createConstantBuffer(const std::wstring_view constantBufferName)
    {
        ConstantBuffer<T> constantBuffer{};

        const D3D12_RESOURCE_DESC constantBufferResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0u,
            .Width = sizeof(T),
            .Height = 1u,
            .DepthOrArraySize = 1u,
            .MipLevels = 1u,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        constantBuffer.buffer = createBuffer(constantBufferResourceDesc, nullptr, constantBufferName);
        const D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferConstantBufferViewDesc = {
            .BufferLocation = constantBuffer.buffer->GetGPUVirtualAddress(),
            .SizeInBytes = sizeof(T),
        };

        m_device->CreateConstantBufferView(&constantBufferConstantBufferViewDesc, m_currentCbvSrvUavDescriptorHeapHandle);
        m_currentCbvSrvUavDescriptorHeapHandle.Offset(m_cbvSrvUavDescriptorSize);

        return constantBuffer;
    }
}
