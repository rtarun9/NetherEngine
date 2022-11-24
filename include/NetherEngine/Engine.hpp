#pragma once

#include "Camera.hpp"

struct SDL_Window;

namespace nether
{
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

        void initMipMapGenerator();

        void initPipelines();
        void initMeshes();
        void initTextures();
        void initScene();

        void executeCopyCommands();

    private:
        // If data is nullptr, a buffer with CPU / GPU access will be created. Else, a GPU only buffer will be created.
        [[nodiscard]] Comptr<ID3D12Resource> createBuffer(const D3D12_RESOURCE_DESC& bufferResourceDesc, const std::byte* data, const std::wstring_view bufferName);

        [[nodiscard]] Texture createTexture(const std::string_view texturePath, const DXGI_FORMAT& format, const std::wstring_view textureName);

        // Helper functions for creating specific types of buffers (structured byffer, index, constant etc). Might be removed eventually, or made into templated functions.
        [[nodiscard]] IndexBuffer createIndexBuffer(const std::byte* data, const uint32_t bufferSize, const std::wstring_view indexBufferName);
        [[nodiscard]] StructuredBuffer createStructuredBuffer(const std::byte* data, const uint32_t numberOfComponents, const uint32_t stride, const std::wstring_view bufferName);
        template <typename T> [[nodiscard]] ConstantBuffer<T> createConstantBuffer(const std::wstring_view constantBufferName);



        [[nodiscard]] Mesh createMesh(const std::string_view meshPath);


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
        uint64_t m_copyFenceValue{};

        uint32_t m_frameIndex{};
        uint32_t m_frameCount{};

        Comptr<ID3D12Fence1> m_fence{};
        HANDLE m_fenceEvent{};

        DescriptorHeap m_rtvDescriptorHeap {};
        DescriptorHeap m_dsvDescriptorHeap{};
        DescriptorHeap m_cbvSrvUavDescriptorHeap{};

        std::array<Comptr<ID3D12Resource>, FRAME_COUNT> m_backBuffers{};
        Comptr<IDXGISwapChain3> m_swapchain{};

        Comptr<ID3D12Resource> m_depthStencilTexture{};

        std::unordered_map<std::wstring, GraphicsPipeline> m_graphicsPipelines{};
        ComputePipeline m_mipMapGenerationPipeline{};

        std::unordered_map<std::wstring, Mesh> m_meshes{};
        std::unordered_map<std::wstring, Texture> m_textures{};

        std::unordered_map<std::wstring, Renderable> m_renderables{};

        Camera m_camera{};
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

        m_device->CreateConstantBufferView(&constantBufferConstantBufferViewDesc, m_cbvSrvUavDescriptorHeap.currentCpuDescriptorHandle);

        constantBuffer.srvIndex = m_cbvSrvUavDescriptorHeap.currentDescriptorIndex;

        m_cbvSrvUavDescriptorHeap.offset();

        return constantBuffer;
    }
}
