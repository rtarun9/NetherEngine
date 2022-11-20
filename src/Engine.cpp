#include "Pch.hpp"

#include "Engine.hpp"

#include "ShaderCompiler.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

namespace nether
{
    Engine::~Engine()
    {
        flushGPU();

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

        // Initialize all the textures.
        initTextures();

        // Initialize meshes.
        initMeshes();

        // Initialize the scene objects (i.e the renderables).
        initScene();

        debugLog(L"Initialized engine.");

        // Main event loop.
        std::chrono::high_resolution_clock clock{};
        std::chrono::high_resolution_clock::time_point previousFrameTime{};

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

                m_camera.handleInput(Keys::W, keyboardState[SDL_SCANCODE_W]);
                m_camera.handleInput(Keys::A, keyboardState[SDL_SCANCODE_A]);
                m_camera.handleInput(Keys::S, keyboardState[SDL_SCANCODE_S]);
                m_camera.handleInput(Keys::D, keyboardState[SDL_SCANCODE_D]);

                m_camera.handleInput(Keys::AUp, keyboardState[SDL_SCANCODE_UP]);
                m_camera.handleInput(Keys::ALeft, keyboardState[SDL_SCANCODE_LEFT]);
                m_camera.handleInput(Keys::ADown, keyboardState[SDL_SCANCODE_DOWN]);
                m_camera.handleInput(Keys::ARight, keyboardState[SDL_SCANCODE_RIGHT]);
            }

            const auto currentFrameTime = clock.now();
            const float deltaTime = static_cast<float>((currentFrameTime - previousFrameTime).count() * 1e-9);
            previousFrameTime = currentFrameTime;

            update(deltaTime);
            render();

            m_frameCount++;
        }
    }

    void Engine::update(const float deltaTime)
    {
        m_camera.update(deltaTime);

        m_renderables[L"Cube"].transformBuffer.data = {
            .modelMatrix = math::XMMatrixIdentity(),
        };

        m_renderables[L"Cube"].transformBuffer.update();

        getCurrentFrameResources().sceneBuffer.data = {
            .viewProjectionMatrix = m_camera.getLookAtMatrix() *
                                    math::XMMatrixPerspectiveFovLH(math::XMConvertToRadians(45.0f), (float)m_windowDimensions.x / (float)m_windowDimensions.y, 0.1f, 100.0f),
        };

        getCurrentFrameResources().sceneBuffer.update();
    }

    void Engine::render()
    {
        // Reset the command list and the command allocator to add new commands.
        throwIfFailed(getCurrentFrameResources().commandAllocator->Reset());
        throwIfFailed(getCurrentFrameResources().commandList->Reset(getCurrentFrameResources().commandAllocator.Get(), nullptr));

        // Alias for the command list.
        Comptr<ID3D12GraphicsCommandList2>& cmd = getCurrentFrameResources().commandList;

        // Set pipeline state.
        const std::array<ID3D12DescriptorHeap*, 1u> shaderVisibleDescriptorHeaps{m_cbvSrvUavDescriptorHeap.Get()};

        // Transition back buffer from presentable state to writable (renderable) state.
        const CD3DX12_RESOURCE_BARRIER presentationToRenderTargetBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmd->ResourceBarrier(1u, &presentationToRenderTargetBarrier);

        // Setup render targets and depth stencil views.
        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
        const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        constexpr std::array<float, 4> clearColor{0.1f, 0.1f, 0.1f, 1.0f};
        cmd->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);
        cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 1u, 0u, nullptr);

        cmd->OMSetRenderTargets(1u, &rtvHandle, FALSE, &dsvHandle);

        cmd->RSSetViewports(1u, &m_viewport);
        cmd->RSSetScissorRects(1u, &m_scissorRect);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        GraphicsPipeline* lastGraphicsPipeline{};
        Mesh* lastMesh{};

        for (const auto& [name, renderable] : m_renderables)
        {
            if (renderable.graphicsPipeline != lastGraphicsPipeline)
            {
                lastGraphicsPipeline = renderable.graphicsPipeline;

                cmd->SetGraphicsRootSignature(lastGraphicsPipeline->rootSignature.Get());
                cmd->SetPipelineState(lastGraphicsPipeline->pipelineState.Get());
                cmd->SetDescriptorHeaps(static_cast<uint32_t>(shaderVisibleDescriptorHeaps.size()), shaderVisibleDescriptorHeaps.data());
            }

            if (lastMesh != renderable.mesh)
            {
                lastMesh = renderable.mesh;

                cmd->IASetVertexBuffers(0u, 1u, &lastMesh->vertexBuffer.vertexBufferView);
                cmd->IASetIndexBuffer(&lastMesh->indexBuffer.indexBufferView);
            }

            cmd->SetGraphicsRootConstantBufferView(lastGraphicsPipeline->rootParameterIndexMap[L"sceneBuffer"],
                                                   getCurrentFrameResources().sceneBuffer.buffer->GetGPUVirtualAddress());

            cmd->SetGraphicsRootConstantBufferView(lastGraphicsPipeline->rootParameterIndexMap[L"transformBuffer"],
                                                   renderable.transformBuffer.buffer->GetGPUVirtualAddress());

            cmd->SetGraphicsRootDescriptorTable(lastGraphicsPipeline->rootParameterIndexMap[L"albedoTexture"],
                                                   m_textures[L"AlbedoTexture"].gpuDescriptorHandle);

            cmd->DrawIndexedInstanced(36u, 1u, 0u, 0u, 0u);
        }

        const CD3DX12_RESOURCE_BARRIER renderTargetToPresentationBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        cmd->ResourceBarrier(1u, &renderTargetToPresentationBarrier);

        throwIfFailed(cmd->Close());

        const std::array<ID3D12CommandList*, 1u> commandLists{cmd.Get()};
        m_directCommandQueue->ExecuteCommandLists(1u, commandLists.data());

        m_swapchain->Present(1u, 0u);

        const uint64_t fenceValue = ++getCurrentFrameResources().fenceValue;

        throwIfFailed(m_directCommandQueue->Signal(m_fence.Get(), fenceValue));

        m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

        if (m_fence->GetCompletedValue() < getCurrentFrameResources().fenceValue)
        {
            throwIfFailed(m_fence->SetEventOnCompletion(getCurrentFrameResources().fenceValue, m_fenceEvent));
            ::WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    void Engine::flushGPU()
    {
        const uint64_t fenceValue = ++getCurrentFrameResources().fenceValue;
        throwIfFailed(m_directCommandQueue->Signal(m_fence.Get(), fenceValue));

        if (m_fence->GetCompletedValue() < fenceValue)
        {
            throwIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
            ::WaitForSingleObject(m_fenceEvent, INFINITE);
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

        // Window must cover 100% of the screen.
        m_windowDimensions = Uint2{
            .x = static_cast<uint32_t>(monitorWidth * 1.0f),
            .y = static_cast<uint32_t>(monitorHeight * 1.0f),
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
        // Initialize core DX12 objects.

        // Enable the debug layer in debug builds.
        if constexpr (NETHER_DEBUG_MODE)
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
            if (NETHER_DEBUG_MODE)
            {
                return DXGI_CREATE_FACTORY_DEBUG;
            }

            return 0;
        }();

        throwIfFailed(::CreateDXGIFactory2(factoryCreationFlags, IID_PPV_ARGS(&m_factory)));

        // Select the highest performance adapter and display its description (adapter represents the display subsystem,
        // i.e GPU, video memory, etc).
        throwIfFailed(m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));

        DXGI_ADAPTER_DESC1 adapterDesc{};
        throwIfFailed(m_adapter->GetDesc1(&adapterDesc));
        std::wcout << "Chosen adapter : " << adapterDesc.Description << L'\n';

        // Create the device (i.e the logical GPU, used in creation of most d3d12 objects).
        throwIfFailed(::D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device)));
        setName(m_device.Get(), L"D3D12 Device");

        // Setup the info queue in debug builds to place breakpoint on invalid API usage.
        if constexpr (NETHER_DEBUG_MODE)
        {
            Comptr<ID3D12InfoQueue1> infoQueue{};
            throwIfFailed(m_device.As(&infoQueue));

            throwIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
            throwIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
            throwIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
        }

        // Create the RTV, DSV, Sampler, CBV_SRV_UAV descriptor heaps.
        initDescriptorHeaps();

        // Create the command objects and synchronization primitives.
        initCommandObjects();
        initSyncPrimitives();

        // Create the swapchain, RTV's and DSV's.
        initSwapchain();
    }

    void Engine::initDescriptorHeaps()
    {
        // Create descriptor heaps (i.e contiguous allocations of descriptors. Descriptors describe some resource and
        // specify extra information about it, how it is to be used, etc.
        const D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = FRAME_COUNT,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
        setName(m_rtvDescriptorHeap.Get(), L"RTV descriptor heap");

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        const D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = 1u,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&m_dsvDescriptorHeap)));
        setName(m_dsvDescriptorHeap.Get(), L"DSV descriptor heap");

        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        const D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDescriptorHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 5u,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateDescriptorHeap(&cbvSrvUavDescriptorHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavDescriptorHeap)));
        setName(m_cbvSrvUavDescriptorHeap.Get(), L"CBV SRV UAV descriptor heap");

        m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_currentCbvSrvUavCPUDescriptorHeapHandle = m_cbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_currentCbvSrvUavGPUDescriptorHeapHandle = m_cbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    }

    void Engine::initCommandObjects()
    {
        // Create the direct command queues (i.e execution ports of the GPU).
        const D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(&m_directCommandQueue)));
        setName(m_directCommandQueue.Get(), L"Direct command queue");

        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            // Create the command allocator (i.e the backing memory store for GPU commands recorded via command lists).
            throwIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameResources[frameIndex].commandAllocator)));
            setName(m_frameResources[frameIndex].commandAllocator.Get(), L"Direct command allocator", frameIndex);

            // Create the command list. Used for recording GPU commands.
            throwIfFailed(m_device->CreateCommandList(0u,
                                                      D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                      m_frameResources[frameIndex].commandAllocator.Get(),
                                                      nullptr,
                                                      IID_PPV_ARGS(&m_frameResources[frameIndex].commandList)));
            setName(m_frameResources[frameIndex].commandList.Get(), L"Direct command list", frameIndex);

            throwIfFailed(m_frameResources[frameIndex].commandList->Close());
        }

        // Reset current command list (so that it can be used for the initial pre rendering setup).
        throwIfFailed(m_frameResources[0].commandList->Reset(m_frameResources[0].commandAllocator.Get(), nullptr));

        // Create copy command objects.
        const D3D12_COMMAND_QUEUE_DESC copyCommandQueueDesc = {
            .Type = D3D12_COMMAND_LIST_TYPE_COPY,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateCommandQueue(&copyCommandQueueDesc, IID_PPV_ARGS(&m_copyCommandQueue)));
        setName(m_copyCommandQueue.Get(), L"Copy command queue");

        throwIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyCommandAllocator)));
        setName(m_copyCommandAllocator.Get(), L"Copy command allocator");

        // Create the command list. Used for recording GPU commands.
        throwIfFailed(m_device->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_COPY, m_copyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_copyCommandList)));

        setName(m_copyCommandList.Get(), L"Copy command list");
    }

    void Engine::initSyncPrimitives()
    {
        // Create the synchronization primitives.
        throwIfFailed(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

        m_fenceEvent = ::CreateEvent(nullptr, false, false, nullptr);

        throwIfFailed(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence)));
    }

    void Engine::initSwapchain()
    {
        // Setup viewport and scissor rect.
        m_viewport = {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(m_windowDimensions.x),
            .Height = static_cast<float>(m_windowDimensions.y),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };

        m_scissorRect = {
            .left = 0u,
            .top = 0u,
            .right = static_cast<long>(m_windowDimensions.x),
            .bottom = static_cast<long>(m_windowDimensions.y),
        };

        // Create the swapchain (allocates backbuffers which we can render into and present to a window).
        const DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
            .Width = m_windowDimensions.x,
            .Height = m_windowDimensions.y,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = {1u, 0u},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = FRAME_COUNT,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = 0u,
        };

        Comptr<IDXGISwapChain1> swapchain{};
        throwIfFailed(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_windowHandle, &swapchainDesc, nullptr, nullptr, &swapchain));
        throwIfFailed(swapchain.As(&m_swapchain));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // Create the render target views for each swapchain backbuffer..
        for (const uint32_t bufferIndex : std::views::iota(0u, FRAME_COUNT))
        {
            throwIfFailed(m_swapchain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_backBuffers[bufferIndex])));
            setName(m_backBuffers[bufferIndex].Get(), L"Back buffer");

            m_device->CreateRenderTargetView(m_backBuffers[bufferIndex].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1u, m_rtvDescriptorSize);
        }

        // Create the Depth stencil buffer and view.
        const D3D12_RESOURCE_DESC depthStencilTextureResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0u,
            .Width = m_windowDimensions.x,
            .Height = m_windowDimensions.y,
            .DepthOrArraySize = 1u,
            .MipLevels = 1u,
            .Format = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        };

        const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        const D3D12_CLEAR_VALUE depthStencilClearValue = {
            .Format = DXGI_FORMAT_D32_FLOAT,
            .DepthStencil =
                {
                    .Depth = 1.0f,
                },
        };

        throwIfFailed(m_device->CreateCommittedResource(&defaultHeapProperties,
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &depthStencilTextureResourceDesc,
                                                        D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                        &depthStencilClearValue,
                                                        IID_PPV_ARGS(&m_depthStencilTexture)));

        // Create the depth stencil texture view.
        const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
            .Format = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D{
                .MipSlice = 0u,
            },
        };

        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle = m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

        m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), &dsvDesc, dsvDescriptorHandle);
    }

    void Engine::initPipelines()
    {
        // Setup the root signature. RS specifies what is the layout that resources are used in the shaders.
        // Root signature parameters are determined by shader reflection.

        // Setup shaders.
        const Shader vertexShader = ShaderCompiler::compile(ShaderTypes::Vertex, L"shaders/TestShader.hlsl", m_graphicsPipelines[L"BasePipeline"]);
        const Shader pixelShader = ShaderCompiler::compile(ShaderTypes::Pixel, L"shaders/TestShader.hlsl", m_graphicsPipelines[L"BasePipeline"]);

        // Setup default (test) static sampler.
        const D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {
            .Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
            .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .MipLODBias = 0,
            .MaxAnisotropy = 0,
            .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
            .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            .MinLOD = 0.0f,
            .MaxLOD = D3D12_FLOAT32_MAX,
            .ShaderRegister = 0,
            .RegisterSpace = 1,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignaureDesc = {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 =
                {
                    .NumParameters = static_cast<uint32_t>(m_graphicsPipelines[L"BasePipeline"].rootParameters.size()),
                    .pParameters = m_graphicsPipelines[L"BasePipeline"].rootParameters.data(),
                    .NumStaticSamplers = 1u,
                    .pStaticSamplers = &staticSamplerDesc,
                    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                },
        };

        m_graphicsPipelines[L"BasePipeline"].vertexShader = vertexShader;
        m_graphicsPipelines[L"BasePipeline"].pixelShader = pixelShader;

        Comptr<ID3DBlob> rootSignatureBlob{};
        Comptr<ID3DBlob> errorBlob{};

        // A serialized root signature is a single chunk of memory
        throwIfFailed(::D3D12SerializeVersionedRootSignature(&rootSignaureDesc, &rootSignatureBlob, &errorBlob));
        if (errorBlob)
        {
            const char* errorMessage = (const char*)errorBlob->GetBufferPointer();
            fatalError(errorMessage);
        }

        throwIfFailed(m_device->CreateRootSignature(0u,
                                                    rootSignatureBlob->GetBufferPointer(),
                                                    rootSignatureBlob->GetBufferSize(),
                                                    IID_PPV_ARGS(&m_graphicsPipelines[L"BasePipeline"].rootSignature)));
        setName(m_graphicsPipelines[L"BasePipeline"].rootSignature.Get(), L"Root signature");

        const D3D12_SHADER_BYTECODE vertexShaderByteCode = {
            .pShaderBytecode = vertexShader.shaderBlob->GetBufferPointer(),
            .BytecodeLength = vertexShader.shaderBlob->GetBufferSize(),
        };

        const D3D12_SHADER_BYTECODE pixelShaderByteCode = {
            .pShaderBytecode = pixelShader.shaderBlob->GetBufferPointer(),
            .BytecodeLength = pixelShader.shaderBlob->GetBufferSize(),

        };

        // Setup graphics pipeline state.
        CD3DX12_RASTERIZER_DESC defaultRasterizerDesc(D3D12_DEFAULT);
        defaultRasterizerDesc.FrontCounterClockwise = false;

        // Setup input layout.
        constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 3u> inputElementDescs = {
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0u,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0u,
                .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0u,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TEXTURE_COORD",
                .SemanticIndex = 0u,
                .Format = DXGI_FORMAT_R32G32_FLOAT,
                .InputSlot = 0u,
                .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0u,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "NORMAL",
                .SemanticIndex = 0u,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0u,
                .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0u,
            },
        };

        const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {
            .pInputElementDescs = inputElementDescs.data(),
            .NumElements = static_cast<uint32_t>(inputElementDescs.size()),
        };

        // Setup depth stencil state.
        const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
            .StencilEnable = false,
        };

        const D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc = {
            .pRootSignature = m_graphicsPipelines[L"BasePipeline"].rootSignature.Get(),
            .VS = vertexShaderByteCode,
            .PS = pixelShaderByteCode,
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT_MAX,
            .RasterizerState = defaultRasterizerDesc,
            .DepthStencilState = depthStencilDesc,
            .InputLayout = inputLayoutDesc,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1u,
            .RTVFormats = DXGI_FORMAT_R8G8B8A8_UNORM,
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {1u, 0u},
            .NodeMask = 0u,
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        throwIfFailed(m_device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&m_graphicsPipelines[L"BasePipeline"].pipelineState)));
        setName(m_graphicsPipelines[L"BasePipeline"].pipelineState.Get(), L"Pipeline State");
    }

    void Engine::initMeshes() { m_meshes[L"CubeMesh"] = createMesh("assets/Cube/glTF/Cube.gltf"); }

    void Engine::initTextures()
    {
        m_textures[L"AlbedoTexture"] = createTexture("assets/Cube/glTF/Cube_BaseColor.png", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, L"Albedo Texture");
    
        // note(rtarun9) : Move this elsewhere.
        // Executes all transition barriers (i.e mostly copy dest -> pixel shader resource view) and flushes the GPU.

        throwIfFailed(getCurrentFrameResources().commandList->Close());
        const std::array<ID3D12CommandList*, 1u> commandLists{getCurrentFrameResources().commandList.Get()};
        m_directCommandQueue->ExecuteCommandLists(1u, commandLists.data());

        m_directCommandQueue->Signal(m_fence.Get(), 1u);
        if (m_fence->GetCompletedValue() < 1u)
        {
            throwIfFailed(m_fence->SetEventOnCompletion(1u, m_fenceEvent));
            ::WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    void Engine::initScene()
    {
        // Create scene buffer (one per frame count).
        for (const uint32_t frameIndex : std::views::iota(0u, FRAME_COUNT))
        {
            m_frameResources[frameIndex].sceneBuffer = createConstantBuffer<SceneData>(L"Scene Buffer");
        }

        m_renderables[L"Cube"].mesh = &m_meshes[L"CubeMesh"];
        m_renderables[L"Cube"].graphicsPipeline = &m_graphicsPipelines[L"BasePipeline"];
        m_renderables[L"Cube"].transformBuffer = createConstantBuffer<TransformData>(L"Cube Transform buffer");
    }

    void Engine::executeCopyCommands()
    {
        throwIfFailed(m_copyCommandList->Close());
        const std::array<ID3D12CommandList*, 1u> copyCommandLists{m_copyCommandList.Get()};
        m_copyCommandQueue->ExecuteCommandLists(1u, copyCommandLists.data());

        m_copyFenceValue++;

        m_copyCommandQueue->Signal(m_copyFence.Get(), m_copyFenceValue);
        if (m_copyFence->GetCompletedValue() < m_copyFenceValue)
        {
            throwIfFailed(m_copyFence->SetEventOnCompletion(m_copyFenceValue, nullptr));
        }

        throwIfFailed(m_copyCommandAllocator->Reset());
        throwIfFailed(m_copyCommandList->Reset(m_copyCommandAllocator.Get(), nullptr));
    };

    Comptr<ID3D12Resource> Engine::createBuffer(const D3D12_RESOURCE_DESC& bufferResourceDesc, const std::byte* data, const std::wstring_view bufferName)
    {
        Comptr<ID3D12Resource> buffer{};

        if (!data)
        {
            // This buffer will have CPU / GPU access. Mostly used for constant buffers.
            const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

            throwIfFailed(
                m_device
                    ->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));
        }
        else
        {
            // Create the buffer in GPU only memory, and create a additional upload buffer that will be in CPU / GPU
            // accesible state. The data from this buffer will be copied into the GPU only buffer.
            const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

            throwIfFailed(
                m_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));

            Comptr<ID3D12Resource> uploadBuffer{};
            const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

            throwIfFailed(m_device->CreateCommittedResource(&uploadHeapProperties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &bufferResourceDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,
                                                            nullptr,
                                                            IID_PPV_ARGS(&uploadBuffer)));
            setName(uploadBuffer.Get(), bufferName.data() + std::wstring(L" upload buffer"));

            // Copy data from data ptr passed in to the CPU / GPU accessible buffer.
            uint8_t* bufferPointer{};

            // Set null read range, as we don't intend on reading from this resource on the CPU.
            const D3D12_RANGE readRange = {
                .Begin = 0u,
                .End = 0u,
            };

            const uint32_t bufferSize = static_cast<uint32_t>(bufferResourceDesc.Width * bufferResourceDesc.Height);

            throwIfFailed(uploadBuffer->Map(0u, &readRange, reinterpret_cast<void**>(&bufferPointer)));
            std::memcpy(bufferPointer, data, bufferSize);
            uploadBuffer->Unmap(0u, nullptr);

            m_copyCommandList->CopyBufferRegion(buffer.Get(), 0u, uploadBuffer.Get(), 0u, bufferSize);

            executeCopyCommands();
        }

        setName(buffer.Get(), bufferName);

        return buffer;
    }

    Texture Engine::createTexture(const std::string_view texturePath, const DXGI_FORMAT& format, const std::wstring_view textureName)
    {
        Texture texture{};

        int32_t componentCount{4u};
        int32_t width{};
        int32_t height{};

        stbi_uc* data = stbi_load(texturePath.data(), &width, &height, nullptr, componentCount);

        // Create the resource desc for this texture.
        const D3D12_RESOURCE_DESC textureResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0u,
            .Width = static_cast<uint64_t>(width),
            .Height = static_cast<uint32_t>(height),
            .DepthOrArraySize = 1u,
            .MipLevels = 1u,
            .Format = format,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        throwIfFailed(m_device->CreateCommittedResource(&defaultHeapProperties,
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &textureResourceDesc,
                                                        D3D12_RESOURCE_STATE_COMMON,
                                                        nullptr,
                                                        IID_PPV_ARGS(&texture.texture)));

        const UINT64 bufferSize = GetRequiredIntermediateSize(texture.texture.Get(), 0, 1);

        // Create a GPU upload buffer for the texture.
        Comptr<ID3D12Resource> uploadBuffer{};
        const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC uploadBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

        throwIfFailed(m_device->CreateCommittedResource(&uploadHeapProperties,
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &uploadBufferResourceDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ,
                                                        nullptr,
                                                        IID_PPV_ARGS(&uploadBuffer)));

        setName(uploadBuffer.Get(), textureName.data() + std::wstring(L" upload buffer"));

        // Place the data on the upload buffer and copy it into the GPU only buffer using the UpdateSubresources() helper function.
        const D3D12_SUBRESOURCE_DATA textureData = {
            .pData = data,
            .RowPitch = width * 4u,
            .SlicePitch = width * height * 4u,
        };

        UpdateSubresources(m_copyCommandList.Get(), texture.texture.Get(), uploadBuffer.Get(), 0u, 0u, 1u, &textureData);
        executeCopyCommands();

        // Transition to pixel shader resource format, as SRV requires texture to be in this format.
        const CD3DX12_RESOURCE_BARRIER copyDestToPixelShaderResourceBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(),
                                                 D3D12_RESOURCE_STATE_COPY_DEST,
                                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        getCurrentFrameResources().commandList->ResourceBarrier(1u, &copyDestToPixelShaderResourceBarrier);

        // Create the shader resource view for the texture.
        const D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {
            .Format = format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D{
                .MipLevels = 1u,
            },
        };

        m_device->CreateShaderResourceView(texture.texture.Get(), &shaderResourceViewDesc, m_currentCbvSrvUavCPUDescriptorHeapHandle);
        m_currentCbvSrvUavCPUDescriptorHeapHandle.Offset(m_cbvSrvUavDescriptorSize);

        texture.gpuDescriptorHandle = m_currentCbvSrvUavGPUDescriptorHeapHandle;
        m_currentCbvSrvUavGPUDescriptorHeapHandle.Offset(m_cbvSrvUavDescriptorSize);

        setName(texture.texture.Get(), textureName);
        return texture;
    }

    VertexBuffer Engine::createVertexBuffer(const std::byte* data, const uint32_t bufferSize, const std::wstring_view vertexBufferName)
    {
        VertexBuffer vertexBuffer{};

        // Setup the resource desc for buffer creation.
        const D3D12_RESOURCE_DESC vertexBufferResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0u,
            .Width = bufferSize,
            .Height = 1u,
            .DepthOrArraySize = 1u,
            .MipLevels = 1u,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        vertexBuffer.buffer = createBuffer(vertexBufferResourceDesc, data, vertexBufferName);

        vertexBuffer.vertexBufferView = {
            .BufferLocation = vertexBuffer.buffer->GetGPUVirtualAddress(),
            .SizeInBytes = bufferSize,
            .StrideInBytes = sizeof(Vertex),
        };

        vertexBuffer.verticesCount = static_cast<uint32_t>(bufferSize / sizeof(Vertex));

        return vertexBuffer;
    }

    IndexBuffer Engine::createIndexBuffer(const std::byte* data, const uint32_t bufferSize, const std::wstring_view indexBufferName)
    {
        IndexBuffer indexBuffer{};

        // Setup the resource desc for buffer creation.
        const D3D12_RESOURCE_DESC indexBufferResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0u,
            .Width = bufferSize,
            .Height = 1u,
            .DepthOrArraySize = 1u,
            .MipLevels = 1u,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        indexBuffer.buffer = createBuffer(indexBufferResourceDesc, data, indexBufferName);

        indexBuffer.indexBufferView = {
            .BufferLocation = indexBuffer.buffer->GetGPUVirtualAddress(),
            .SizeInBytes = bufferSize,
            .Format = DXGI_FORMAT_R32_UINT,
        };

        return indexBuffer;
    }

    Mesh Engine::createMesh(const std::string_view modelPath)
    {
        // Use tinygltf loader to load the model.

        const std::string fullModelPath = modelPath.data();

        std::string warning{};
        std::string error{};

        tinygltf::TinyGLTF context{};

        tinygltf::Model model{};

        if (!context.LoadASCIIFromFile(&model, &error, &warning, fullModelPath))
        {
            if (!error.empty())
            {
                fatalError(error);
            }

            if (!warning.empty())
            {
                fatalError(warning);
            }
        }

        // Build meshes.
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];

        tinygltf::Node& node = model.nodes[0u];
        node.mesh = std::max<int32_t>(0u, node.mesh);

        const tinygltf::Mesh& nodeMesh = model.meshes[node.mesh];

        // note(rtarun9) : for now have all vertices in single vertex buffer. Same for index buffer.
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};

        for (size_t i = 0; i < nodeMesh.primitives.size(); ++i)
        {

            // Reference used :
            // https://github.com/mateeeeeee/Adria-DX12/blob/fc98468095bf5688a186ca84d94990ccd2f459b0/Adria/Rendering/EntityLoader.cpp.

            // Get Accessors, buffer view and buffer for each attribute (position, textureCoord, normal).
            tinygltf::Primitive primitive = nodeMesh.primitives[i];
            const tinygltf::Accessor& indexAccesor = model.accessors[primitive.indices];

            // Position data.
            const tinygltf::Accessor& positionAccesor = model.accessors[primitive.attributes["POSITION"]];
            const tinygltf::BufferView& positionBufferView = model.bufferViews[positionAccesor.bufferView];
            const tinygltf::Buffer& positionBuffer = model.buffers[positionBufferView.buffer];

            const int positionByteStride = positionAccesor.ByteStride(positionBufferView);
            uint8_t const* const positions = &positionBuffer.data[positionBufferView.byteOffset + positionAccesor.byteOffset];

            // TextureCoord data.
            const tinygltf::Accessor& textureCoordAccesor = model.accessors[primitive.attributes["TEXCOORD_0"]];
            const tinygltf::BufferView& textureCoordBufferView = model.bufferViews[textureCoordAccesor.bufferView];
            const tinygltf::Buffer& textureCoordBuffer = model.buffers[textureCoordBufferView.buffer];
            const int textureCoordBufferStride = textureCoordAccesor.ByteStride(textureCoordBufferView);
            uint8_t const* const texcoords = &textureCoordBuffer.data[textureCoordBufferView.byteOffset + textureCoordAccesor.byteOffset];

            // Normal data.
            const tinygltf::Accessor& normalAccesor = model.accessors[primitive.attributes["NORMAL"]];
            const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccesor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
            const int normalByteStride = normalAccesor.ByteStride(normalBufferView);
            uint8_t const* const normals = &normalBuffer.data[normalBufferView.byteOffset + normalAccesor.byteOffset];

            // Fill in the vertices's array.
            for (size_t i : std::views::iota(0u, positionAccesor.count))
            {
                const math::XMFLOAT3 position{(reinterpret_cast<float const*>(positions + (i * positionByteStride)))[0],
                                              (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[1],
                                              (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[2]};

                const math::XMFLOAT2 textureCoord{
                    (reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[0],
                    (reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[1],
                };

                const math::XMFLOAT3 normal{
                    (reinterpret_cast<float const*>(normals + (i * normalByteStride)))[0],
                    (reinterpret_cast<float const*>(normals + (i * normalByteStride)))[1],
                    (reinterpret_cast<float const*>(normals + (i * normalByteStride)))[2],
                };

                // note(rtarun9) : using normals as colors for now, until texture loading is implemented.
                vertices.emplace_back(Vertex{position, textureCoord, normal});
            }

            // Get the index buffer data.
            const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccesor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
            const int indexByteStride = indexAccesor.ByteStride(indexBufferView);
            uint8_t const* const indexes = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccesor.byteOffset;

            // Fill indices array.
            for (size_t i : std::views::iota(0u, indexAccesor.count))
            {
                if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    indices.push_back(static_cast<uint32_t>((reinterpret_cast<uint16_t const*>(indexes + (i * indexByteStride)))[0]));
                }
                else if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    indices.push_back(static_cast<uint32_t>((reinterpret_cast<uint32_t const*>(indexes + (i * indexByteStride)))[0]));
                }
            }
        }

        const uint32_t indexBufferSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
        const uint32_t vertexBufferSize = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));

        Mesh mesh{};
        mesh.indexBuffer = createIndexBuffer(reinterpret_cast<const std::byte*>(indices.data()), indexBufferSize, stringToWString(modelPath) + std::wstring(L" Index buffer"));
        mesh.vertexBuffer = createVertexBuffer(reinterpret_cast<const std::byte*>(vertices.data()), vertexBufferSize, stringToWString(modelPath) + std::wstring(L" Vertex buffer"));

        return mesh;
    }
}
