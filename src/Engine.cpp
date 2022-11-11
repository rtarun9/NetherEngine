#include "Pch.hpp"

#include "Engine.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

namespace nether
{
    Engine::~Engine()
    {
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    void Engine::run()
    {
        // Initialize and create the SDL2 window.
        initPlatformBackend();

        // Initialize the core DirectX12 and DXGI structures.
        initGraphicsBackend();

        // Initialize and create all the pipelines and root signatures.
        initPipelines();

        debugLog(L"Initialized engine.");

        // Main event loop.
        bool quit{false};
        while (!quit)
        {
            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }

                const uint8_t* keyboardState = SDL_GetKeyboardState(nullptr);
                if (keyboardState[SDL_SCANCODE_ESCAPE])
                {
                    quit = true;
                }
            }
        }
    }

    void Engine::initPlatformBackend()
    {
        // Initialize SDL2 and create window.
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            fatalError("Failed to initialize SDL2.");
        }

        // Set DPI awareness on Windows
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");

        // Get monitor dimensions.
        SDL_DisplayMode displayMode{};
        if (SDL_GetCurrentDisplayMode(0, &displayMode) < 0)
        {
            fatalError("Failed to get display mode");
        }

        const uint32_t monitorWidth = displayMode.w;
        const uint32_t monitorHeight = displayMode.h;

        // Window must cover 85% of the screen.
        m_windowDimensions = Uint2{
            .x = static_cast<uint32_t>(monitorWidth * 0.85f),
            .y = static_cast<uint32_t>(monitorHeight * 0.85f),
        };

        m_window = SDL_CreateWindow("LunarEngine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_windowDimensions.x, m_windowDimensions.y, SDL_WINDOW_ALLOW_HIGHDPI);
        if (!m_window)
        {
            fatalError("Failed to create SDL2 window.");
        }

        SDL_SysWMinfo wmInfo{};
        SDL_VERSION(&wmInfo.version);

        SDL_GetWindowWMInfo(m_window, &wmInfo);
        m_windowHandle = wmInfo.info.win.window;
    }
    void Engine::initGraphicsBackend()
    {
        // Enable the debug layer in debug builds.
        if constexpr (NETHER_DEUBG_MODE)
        {
            Comptr<ID3D12Debug5> debugController{};
            throwIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));

            debugController->EnableDebugLayer();

            debugController->SetEnableAutoName(true);
            debugController->SetEnableGPUBasedValidation(true);
            debugController->SetEnableSynchronizedCommandQueueValidation(true);
        }

        // Create the DXGI factory for querying adapters and aid in creation of other DXGI objects.
        // Set the DXGI_CREATE_FACTORY_DEBUG in debug mode.
        constexpr uint32_t factoryCreationFlags = [=]()
        {
            if (NETHER_DEUBG_MODE)
            {
                return DXGI_CREATE_FACTORY_DEBUG;
            }

            return 0;
        }();

        throwIfFailed(::CreateDXGIFactory2(factoryCreationFlags, IID_PPV_ARGS(&m_factory)));

        // Select the highest performance adapter and display its description (adapter represents the display subsystem, i.e GPU, video memory, etc).
        throwIfFailed(m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));

        DXGI_ADAPTER_DESC1 adapterDesc{};
        throwIfFailed(m_adapter->GetDesc1(&adapterDesc));
        std::wcout << "Chosen adapter : " << adapterDesc.Description << L'\n';

        // Create the device (i.e the logical GPU, used in creation of most d3d12 objects).
        throwIfFailed(::D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device)));
        setName(m_device.Get(), L"D3D12 Device");

        // Setup the info queue in debug builds to place breakpoint on invalid API usage.
        if constexpr (NETHER_DEUBG_MODE)
        {
            Comptr<ID3D12InfoQueue1> infoQueue{};
            throwIfFailed(m_device.As(&infoQueue));

            throwIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
            throwIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
            throwIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
        }

        // Create the command queues (i.e execution ports of the GPU).
        const D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(&m_directCommandQueue)));
        setName(m_directCommandQueue.Get(), L"Direct command queue");

        // Create the command allocator (i.e the backing memory store for GPU commands recorded via command lists).
        throwIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
        setName(m_commandAllocator.Get(), L"Direct command allocator");

        // Create the command list. Used for recording GPU commands.
        throwIfFailed(m_device->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
        setName(m_commandList.Get(), L"Direct command list");

        // Create descriptor heaps (i.e contiguous allocations of descriptors. Descriptors describe some resource and specify extra information about it, how it is to be used, etc.
        const D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = BACK_BUFFER_COUNT,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
        setName(m_rtvDescriptorHeap.Get(), L"RTV descriptor heap");

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Create the swapchain (allocates backbuffers which we can render into and present to a window).
        const DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
            .Width = m_windowDimensions.x,
            .Height = m_windowDimensions.y,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = {1u, 0u},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = BACK_BUFFER_COUNT,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = 0u,
        };

        throwIfFailed(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_windowHandle, &swapchainDesc, nullptr, nullptr, &m_swapchain));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // Create the render target views for each swapchain backbuffer.
        for (const uint32_t bufferIndex : std::views::iota(0u, BACK_BUFFER_COUNT))
        {
            throwIfFailed(m_swapchain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_backBuffers[bufferIndex])));
            setName(m_backBuffers[bufferIndex].Get(), L"Back buffer");

            m_device->CreateRenderTargetView(m_backBuffers[bufferIndex].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1u, m_rtvDescriptorSize);
        }
    }

    void Engine::initPipelines()
    {
        // Setup the root signature. RS specifies what is the layout that resources are used in the shaders.
        // Empty for now.

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignaureDesc = {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 =
                {
                    .NumParameters = 0u,
                    .pParameters = nullptr,
                    .NumStaticSamplers = 0u,
                    .pStaticSamplers = nullptr,
                    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                },
        };

        Comptr<ID3DBlob> rootSignatureBlob{};
        Comptr<ID3DBlob> errorBlob{};

        throwIfFailed(::D3D12SerializeVersionedRootSignature(&rootSignaureDesc, &rootSignatureBlob, &errorBlob));
        if (errorBlob)
        {
            const char* errorMessage = (const char*)errorBlob->GetBufferPointer();
            fatalError(errorMessage);
        }

        throwIfFailed(m_device->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_pipeline.rootSignature)));
        setName(m_pipeline.rootSignature.Get(), L"Root signature");
    }
}
