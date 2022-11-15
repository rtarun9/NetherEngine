#include "Pch.hpp"

#include "Engine.hpp"

#include "ShaderCompiler.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

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

        // Initialize meshes.
        initMeshes();

        // Upload all buffers and execute copy command queue.
        uploadBuffers();

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
            }

            const auto currentFrameTime = clock.now();
            const float deltaTime = static_cast<float>((currentFrameTime - previousFrameTime).count());
            previousFrameTime = currentFrameTime;

            update(deltaTime);
            render();

            m_frameCount++;
        }
    }

    void Engine::update(const float deltaTime) {}

    void Engine::render()
    {
        // Reset the command list and the command allocator to add new commands.
        throwIfFailed(getCurrentFrameResources().commandAllocator->Reset());
        throwIfFailed(getCurrentFrameResources().commandList->Reset(getCurrentFrameResources().commandAllocator.Get(), nullptr));

        // Alias for the command list.
        Comptr<ID3D12GraphicsCommandList2>& cmd = getCurrentFrameResources().commandList;

        // Set pipeline state.
        const std::array<ID3D12DescriptorHeap*, 1u> shaderVisibleDescriptorHeaps{};

        cmd->SetGraphicsRootSignature(m_pipeline.rootSignature.Get());
        cmd->SetPipelineState(m_pipeline.pipelineState.Get());

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

        cmd->IASetVertexBuffers(0u, 1u, &m_triangleMesh.vertexBuffer.vertexBufferView);
        cmd->IASetIndexBuffer(&m_triangleMesh.indexBuffer.indexBufferView);
        cmd->SetGraphicsRootConstantBufferView(0u, m_transformBuffer.buffer->GetGPUVirtualAddress());

        cmd->DrawIndexedInstanced(36u, 1u, 0u, 0u, 0u);

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
        // Initialize core DX12 objects.

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
        if constexpr (NETHER_DEUBG_MODE)
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
        m_currentCbvSrvUavDescriptorHeapHandle = m_cbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
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

        // Create the render target views for each swapchain backbuffer.
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
        // One cbv inline descriptor.
        const D3D12_ROOT_PARAMETER1 transformBufferRootParameter = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor =
                {
                    .ShaderRegister = 0u,
                    .RegisterSpace = 0u,
                    .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX,
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignaureDesc = {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 =
                {
                    .NumParameters = 1u,
                    .pParameters = &transformBufferRootParameter,
                    .NumStaticSamplers = 0u,
                    .pStaticSamplers = nullptr,
                    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                },
        };

        Comptr<ID3DBlob> rootSignatureBlob{};
        Comptr<ID3DBlob> errorBlob{};

        // A serialized root signature is a single chunk of memory
        throwIfFailed(::D3D12SerializeVersionedRootSignature(&rootSignaureDesc, &rootSignatureBlob, &errorBlob));
        if (errorBlob)
        {
            const char* errorMessage = (const char*)errorBlob->GetBufferPointer();
            fatalError(errorMessage);
        }

        throwIfFailed(m_device->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_pipeline.rootSignature)));
        setName(m_pipeline.rootSignature.Get(), L"Root signature");

        // Setup shaders.
        const Shader vertexShader = ShaderCompiler::compile(ShaderTypes::vertex, L"shaders/TriangleShader.hlsl");
        const Shader pixelShader = ShaderCompiler::compile(ShaderTypes::pixel, L"shaders/TriangleShader.hlsl");

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
        constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 2u> inputElementDescs = {
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
                .SemanticName = "COLOR",
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
            .pRootSignature = m_pipeline.rootSignature.Get(),
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

        throwIfFailed(m_device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&m_pipeline.pipelineState)));
        setName(m_pipeline.pipelineState.Get(), L"Pipeline State");
    }

    void Engine::initMeshes()
    {
        constexpr std::array<Vertex, 8> triangleVertices{
            Vertex{.position = math::XMFLOAT3(-1.0f, -1.0f, -1.0f), .color = math::XMFLOAT3(0.0f, 0.0f, 0.0f)},
            Vertex{.position = math::XMFLOAT3(-1.0f, 1.0f, -1.0f), .color = math::XMFLOAT3(0.0f, 1.0f, 0.0f)},
            Vertex{.position = math::XMFLOAT3(1.0f, 1.0f, -1.0f), .color = math::XMFLOAT3(1.0f, 1.0f, 0.0f)},
            Vertex{.position = math::XMFLOAT3(1.0f, -1.0f, -1.0f), .color = math::XMFLOAT3(1.0f, 0.0f, 0.0f)},
            Vertex{.position = math::XMFLOAT3(-1.0f, -1.0f, 1.0f), .color = math::XMFLOAT3(0.0f, 0.0f, 1.0f)},
            Vertex{.position = math::XMFLOAT3(-1.0f, 1.0f, 1.0f), .color = math::XMFLOAT3(0.0f, 1.0f, 1.0f)},
            Vertex{.position = math::XMFLOAT3(1.0f, 1.0f, 1.0f), .color = math::XMFLOAT3(1.0f, 1.0f, 1.0f)},
            Vertex{.position = math::XMFLOAT3(1.0f, -1.0f, 1.0f), .color = math::XMFLOAT3(1.0f, 0.0f, 1.0f)},
        };

        constexpr uint32_t vertexBufferSize = static_cast<uint32_t>(sizeof(Vertex) * triangleVertices.size());

        m_triangleMesh.vertexBuffer = createVertexBuffer(reinterpret_cast<const std::byte*>(triangleVertices.data()), vertexBufferSize, L"Triangle Vertex Buffer");

        constexpr std::array<uint32_t, 36> triangleIndices{0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7};
        constexpr uint32_t indexBufferSize = static_cast<uint32_t>(sizeof(uint32_t) * triangleIndices.size());

        m_triangleMesh.indexBuffer = createIndexBuffer(reinterpret_cast<const std::byte*>(triangleIndices.data()), indexBufferSize, L"Triangle Index Buffer");

        m_transformBuffer = createConstantBuffer<TransformData>(L"Transform buffer");

        static const math::XMVECTOR eyePosition = math::XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f);
        static const math::XMVECTOR targetPosition = math::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        static const math::XMVECTOR upDirection = math::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        m_transformBuffer.data = {
            math::XMMatrixLookAtLH(eyePosition, targetPosition, upDirection) *
                math::XMMatrixPerspectiveFovLH(math::XMConvertToRadians(45.0f), (float)m_windowDimensions.x / (float)m_windowDimensions.y, 0.1f, 100.0f),
        };

        m_transformBuffer.update();
    }

    void Engine::uploadBuffers()
    {
        throwIfFailed(m_copyCommandList->Close());
        const std::array<ID3D12CommandList*, 1u> copyCommandLists{m_copyCommandList.Get()};
        m_copyCommandQueue->ExecuteCommandLists(1u, copyCommandLists.data());

        m_copyCommandQueue->Signal(m_copyFence.Get(), 1u);
        if (m_copyFence->GetCompletedValue() < 1u)
        {
            throwIfFailed(m_copyFence->SetEventOnCompletion(1u, m_fenceEvent));
            ::WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_uploadBuffers.clear();
    }

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

            m_uploadBuffers.emplace_back(uploadBuffer);
        }

        setName(buffer.Get(), bufferName);

        return buffer;
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
}
