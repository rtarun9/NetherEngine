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

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl.h>

// Setup the Agility SDK parameters.
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 602u;
}
extern "C"
{
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

namespace nether
{
    Engine::~Engine()
    {
        flushGPU();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    void Engine::run()
    {
        // Initialize and create the SDL2 window.
        initPlatformBackend();

        // Initialize the core DirectX12 and DXGI structures.
        initGraphicsBackend();

        // Initialize ImGui.
        initImgui();

        // Initialize and create all the pipelines and root signatures.
        initPipelines();

        // Initialize mip map generator.
        initMipMapGenerator();

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
                ImGui_ImplSDL2_ProcessEvent(&event);

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

        for (auto& [name, renderable] : m_renderables)
        {
            renderable.transformBuffer.data = {
                .modelMatrix = math::XMMatrixRotationX(renderable.rotate.x) * math::XMMatrixRotationY(renderable.rotate.y) * math::XMMatrixRotationZ(renderable.rotate.z) *
                               math::XMMatrixScaling(renderable.scale.x, renderable.scale.y, renderable.scale.z) *
                               math::XMMatrixTranslation(renderable.translate.x, renderable.translate.y, renderable.translate.z),
            };

            renderable.transformBuffer.data.inverseModelViewMatrix =
                math::XMMatrixInverse(nullptr, math::XMMatrixMultiply(renderable.transformBuffer.data.modelMatrix, m_camera.getLookAtMatrix()));

            renderable.transformBuffer.update();
        }

        // Calculate viewspace light position.
        const math::XMVECTOR lightPosition = math::XMLoadFloat3(&m_renderables[L"Light"].translate);
        const math::XMVECTOR viewSpaceLightPosition = math::XMVector3TransformCoord(lightPosition, m_camera.getLookAtMatrix());
        math::XMFLOAT3 viewSpaceLightPositionFloat3{};
        math::XMStoreFloat3(&viewSpaceLightPositionFloat3, viewSpaceLightPosition);

        const math::XMVECTOR directionalLightPosition = math::XMLoadFloat3(&m_directionalLightPosition);
        const math::XMVECTOR directionalViewSpaceLightPosition = math::XMVector3TransformCoord(directionalLightPosition, m_camera.getLookAtMatrix());
        math::XMFLOAT3 directionalViewSpaceLightPositionFloat3{};
        math::XMStoreFloat3(&directionalViewSpaceLightPositionFloat3, directionalViewSpaceLightPosition);

        getCurrentFrameResources().sceneBuffer.data = {
            .viewMatrix = m_camera.getLookAtMatrix(),
            .viewProjectionMatrix = m_camera.getLookAtMatrix() *
                                    math::XMMatrixPerspectiveFovLH(math::XMConvertToRadians(45.0f), (float)m_windowDimensions.x / (float)m_windowDimensions.y, 0.1f, 100.0f),
            .lightColor = getCurrentFrameResources().sceneBuffer.data.lightColor,
            .viewSpaceLightPosition = viewSpaceLightPositionFloat3,

            .directionalLightColor = m_directionalLightColor,
            .viewSpaceDirectionalLightPosition = directionalViewSpaceLightPositionFloat3,
        };

        getCurrentFrameResources().sceneBuffer.data.viewMatrix = m_camera.getLookAtMatrix();

        getCurrentFrameResources().sceneBuffer.update();
    }

    void Engine::render()
    {
        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Add ImGui render stuff here.
        ImGui::Begin("Renderables");
        for (auto& [name, renderable] : m_renderables)
        {
            const std::string nameStr = wStringToString(name);
            if (ImGui::TreeNode(nameStr.c_str()))
            {
                ImGui::SliderFloat3("rotate", &renderable.rotate.x, -math::XMConvertToRadians(90.0f), math::XMConvertToRadians(90.0f));
                ImGui::SliderFloat("scale", &renderable.scale.x, 0.1f, 10.0f);
                ImGui::SliderFloat3("translate", &renderable.translate.x, -25.0f, 25.0f);
                ImGui::TreePop();

                // scale is applied uniformly to x, y, and z components.
                renderable.scale.y = renderable.scale.x;
                renderable.scale.z = renderable.scale.x;
            }
        }
        ImGui::End();

        ImGui::Begin("Scene data");
        ImGui::ColorEdit3("light color", &getCurrentFrameResources().sceneBuffer.data.lightColor.x);
        ImGui::ColorEdit3("directional light color", &m_directionalLightColor.x);
        ImGui::SliderFloat3("directional light position", &m_directionalLightPosition.x, -25.0f, 25.0f);

        ImGui::End();

        ImGui::Render();

        // Reset the command list and the command allocator to add new commands.
        throwIfFailed(getCurrentFrameResources().commandAllocator->Reset());
        throwIfFailed(getCurrentFrameResources().commandList->Reset(getCurrentFrameResources().commandAllocator.Get(), nullptr));

        // Alias for the command list.
        Comptr<ID3D12GraphicsCommandList2>& cmd = getCurrentFrameResources().commandList;

        // Set pipeline state.
        const std::array<ID3D12DescriptorHeap*, 1u> shaderVisibleDescriptorHeaps{m_cbvSrvUavDescriptorHeap.descriptorHeap.Get()};

        // Transition back buffer from presentable state to writable (renderable) state.
        const CD3DX12_RESOURCE_BARRIER presentationToRenderTargetBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmd->ResourceBarrier(1u, &presentationToRenderTargetBarrier);

        // Setup render targets and depth stencil views.
        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap.getCpuDescriptorHandleAtIndex(m_frameIndex));
        const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvDescriptorHeap.cpuDescriptorHandleFromHeapStart);

        constexpr std::array<float, 4> clearColor{0.1f, 0.1f, 0.1f, 1.0f};
        cmd->ClearRenderTargetView(rtvHandle, clearColor.data(), 0u, nullptr);
        cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 1u, 0u, nullptr);

        cmd->OMSetRenderTargets(1u, &rtvHandle, FALSE, &dsvHandle);

        cmd->RSSetViewports(1u, &m_viewport);
        cmd->RSSetScissorRects(1u, &m_scissorRect);

        GraphicsPipeline* lastGraphicsPipeline{};
        Mesh* lastMesh{};

        for (const auto& [name, renderable] : m_renderables)
        {
            if (renderable.graphicsPipeline != lastGraphicsPipeline)
            {
                lastGraphicsPipeline = renderable.graphicsPipeline;

                cmd->SetDescriptorHeaps(static_cast<uint32_t>(shaderVisibleDescriptorHeaps.size()), shaderVisibleDescriptorHeaps.data());

                cmd->SetGraphicsRootSignature(m_bindlessRootSignature.Get());
                cmd->SetPipelineState(lastGraphicsPipeline->pipelineState.Get());
            }

            if (lastMesh != renderable.mesh)
            {
                lastMesh = renderable.mesh;

                cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                cmd->IASetIndexBuffer(&lastMesh->indexBuffer.indexBufferView);
            }

            struct RenderResources
            {
                uint32_t positionBufferIndex{};
                uint32_t textureCoordBufferIndex{};
                uint32_t normalBufferIndex{};

                uint32_t sceneBufferIndex{};
                uint32_t transformBufferIndex{};

                uint32_t albedoTextureIndex{};
            };

            const RenderResources renderResources = {
                .positionBufferIndex = lastMesh->positionBuffer.srvIndex,
                .textureCoordBufferIndex = lastMesh->textureCoordBuffer.srvIndex,
                .normalBufferIndex = lastMesh->normalBuffer.srvIndex,

                .sceneBufferIndex = getCurrentFrameResources().sceneBuffer.cbvIndex,
                .transformBufferIndex = renderable.transformBuffer.cbvIndex,

                .albedoTextureIndex = m_textures[L"AlbedoTexture"].srvIndex,
            };

            cmd->SetGraphicsRoot32BitConstants(0u, 64u, &renderResources, 0u);

            cmd->DrawIndexedInstanced(36u, 1u, 0u, 0u, 0u);
        }

        if (m_showUI)
        {
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd.Get());
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
        m_rtvDescriptorHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, FRAME_COUNT, L"RTV Descriptor Heap");
        m_dsvDescriptorHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1u, L"DSV Descriptor Heap");
        m_cbvSrvUavDescriptorHeap.init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 250u, L"CBV SRV UAV Descriptor Heap");
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

        // Create compute command objects.
        const D3D12_COMMAND_QUEUE_DESC computeCommandQueueDesc = {
            .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(m_device->CreateCommandQueue(&computeCommandQueueDesc, IID_PPV_ARGS(&m_computeCommandQueue)));
        setName(m_computeCommandQueue.Get(), L"Compute command queue");

        throwIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeCommandAllocator)));
        setName(m_computeCommandAllocator.Get(), L"Compute command allocator");

        // Create the command list. Used for recording GPU commands.
        throwIfFailed(m_device->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_computeCommandList)));

        setName(m_computeCommandList.Get(), L"Compute command list");
    }

    void Engine::initSyncPrimitives()
    {
        // Create the synchronization primitives.
        throwIfFailed(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

        m_fenceEvent = ::CreateEvent(nullptr, false, false, nullptr);

        throwIfFailed(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence)));

        throwIfFailed(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence)));
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

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHeap.cpuDescriptorHandleFromHeapStart;

        // Create the render target views for each swapchain backbuffer..
        for (const uint32_t bufferIndex : std::views::iota(0u, FRAME_COUNT))
        {
            throwIfFailed(m_swapchain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_backBuffers[bufferIndex])));
            setName(m_backBuffers[bufferIndex].Get(), L"Back buffer");

            m_device->CreateRenderTargetView(m_backBuffers[bufferIndex].Get(), nullptr, rtvHandle);
            m_rtvDescriptorHeap.offset(rtvHandle);
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

        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle = m_dsvDescriptorHeap.currentCpuDescriptorHandle;

        m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), &dsvDesc, dsvDescriptorHandle);
    }

    void Engine::initImgui()
    {
        // Setup Dear ImGui context.
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // Setup Dear ImGui style.
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends.
        ImGui_ImplSDL2_InitForD3D(m_window);

        ImGui_ImplDX12_Init(m_device.Get(),
                            FRAME_COUNT,
                            DXGI_FORMAT_R8G8B8A8_UNORM,
                            m_cbvSrvUavDescriptorHeap.descriptorHeap.Get(),
                            m_cbvSrvUavDescriptorHeap.currentCpuDescriptorHandle,
                            m_cbvSrvUavDescriptorHeap.currentGpuDescriptorHandle);

        m_cbvSrvUavDescriptorHeap.offset();
    }

    void Engine::initPipelines()
    {
        // Setup all static samplers (see shaders/StaticSampler.hlsli).
        const std::array<CD3DX12_STATIC_SAMPLER_DESC, 5> staticSamplers = {
            // pointClampSampler.
            CD3DX12_STATIC_SAMPLER_DESC(0u,
                                        D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
                                        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                        D3D12_TEXTURE_ADDRESS_MODE_CLAMP),

            // pointWrapSampler.
            CD3DX12_STATIC_SAMPLER_DESC(1u,
                                        D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP),

            // linearClampSampler.
            CD3DX12_STATIC_SAMPLER_DESC(2u, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),

            // linearWrapSampler.
            CD3DX12_STATIC_SAMPLER_DESC(3u,
                                        D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP),

            // anisotropicSampler.
            CD3DX12_STATIC_SAMPLER_DESC(4u, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
        };

        // Setup the root signature. RS specifies what is the layout that resources are used in the shaders. As bindless is used, there is only one root signature for all
        // pipelines.
        const D3D12_ROOT_PARAMETER1 rootParameters = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants =
                {
                    .ShaderRegister = 0u,
                    .RegisterSpace = 0u,
                    .Num32BitValues = 64,
                },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignaureDesc = {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 =
                {
                    .NumParameters = 1u,
                    .pParameters = &rootParameters,
                    .NumStaticSamplers = static_cast<uint32_t>(staticSamplers.size()),
                    .pStaticSamplers = staticSamplers.data(),
                    .Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED,
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

        throwIfFailed(m_device->CreateRootSignature(0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_bindlessRootSignature)));
        setName(m_bindlessRootSignature.Get(), L"Bindless Root signature");

        createPipeline(L"shaders/PhongShader.hlsl", L"shaders/PhongShader.hlsl", L"BasePipeline");
        createPipeline(L"shaders/LightShader.hlsl", L"shaders/LightShader.hlsl", L"LightPipeline");
    }

    void Engine::initMipMapGenerator()
    {
        // Setup the mip map generation compute pipeline.

        // Setup shaders.
        const Shader computeShader = ShaderCompiler::compile(ShaderTypes::Compute, L"shaders/GenerateMipMaps.hlsl");

        const D3D12_SHADER_BYTECODE computeShaderByteCode = {
            .pShaderBytecode = computeShader.shaderBlob->GetBufferPointer(),
            .BytecodeLength = computeShader.shaderBlob->GetBufferSize(),
        };

        const D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {
            .pRootSignature = m_bindlessRootSignature.Get(),
            .CS = computeShaderByteCode,
        };

        throwIfFailed(m_device->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&m_mipMapGenerationPipeline.pipelineState)));
        setName(m_mipMapGenerationPipeline.pipelineState.Get(), std::wstring(L" Mip Map Generation Compute Pipeline State"));

        m_mipGenBuffer = createConstantBuffer<GenerateMipMapData>(L"Mip Map Generate Constant Buffer");
    }

    void Engine::initMeshes() { m_meshes[L"CubeMesh"] = createMesh("assets/Cube/glTF/Cube.gltf"); }

    void Engine::initTextures() { m_textures[L"AlbedoTexture"] = createTexture("assets/Cube/glTF/Cube_BaseColor.png", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, L"Albedo Texture"); }

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

        m_renderables[L"Light"].mesh = &m_meshes[L"CubeMesh"];
        m_renderables[L"Light"].graphicsPipeline = &m_graphicsPipelines[L"LightPipeline"];
        m_renderables[L"Light"].transformBuffer = createConstantBuffer<TransformData>(L"Cube Transform buffer");
        m_renderables[L"Light"].translate = math::XMFLOAT3{2.0f, 2.0f, 0.0f};
        m_renderables[L"Light"].scale = math::XMFLOAT3{0.3f, 0.3f, 0.3f};
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

    void Engine::executeComputeCommands()
    {
        throwIfFailed(m_computeCommandList->Close());
        const std::array<ID3D12CommandList*, 1u> computeCommandLists{m_computeCommandList.Get()};
        m_computeCommandQueue->ExecuteCommandLists(1u, computeCommandLists.data());

        m_computeFenceValue++;

        m_computeCommandQueue->Signal(m_computeFence.Get(), m_computeFenceValue);
        if (m_computeFence->GetCompletedValue() < m_computeFenceValue)
        {
            throwIfFailed(m_computeFence->SetEventOnCompletion(m_computeFenceValue, nullptr));
        }

        throwIfFailed(m_computeCommandAllocator->Reset());
        throwIfFailed(m_computeCommandList->Reset(m_computeCommandAllocator.Get(), nullptr));
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

    Texture Engine::createTexture(const std::string_view texturePath, const DXGI_FORMAT& format, const bool generateMipMaps, const std::wstring_view textureName)
    {
        Texture texture{};

        int32_t componentCount{4u};
        int32_t width{};
        int32_t height{};

        stbi_uc* data = stbi_load(texturePath.data(), &width, &height, nullptr, componentCount);

        uint16_t mipLevels = 1u;
        if (generateMipMaps)
        {
            mipLevels = static_cast<uint16_t>(std::floor(std::log2(std::max<int32_t>(width, height))));

            if (mipLevels >= width)
            {
                mipLevels = static_cast<UINT16>(width - 1);
            }

            if (mipLevels >= height)
            {
                mipLevels = static_cast<UINT16>(height - 1);
            }
        }

        // Create the resource desc for this texture.
        const D3D12_RESOURCE_DESC textureResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0u,
            .Width = static_cast<uint64_t>(width),
            .Height = static_cast<uint32_t>(height),
            .DepthOrArraySize = 1u,
            .MipLevels = mipLevels,
            .Format = format,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
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
            CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // Create the shader resource view for the texture.
        const D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {
            .Format = format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D{
                .MipLevels = 1u,
            },
        };

        m_device->CreateShaderResourceView(texture.texture.Get(), &shaderResourceViewDesc, m_cbvSrvUavDescriptorHeap.currentCpuDescriptorHandle);

        texture.gpuDescriptorHandle = m_cbvSrvUavDescriptorHeap.currentGpuDescriptorHandle;
        texture.srvIndex = m_cbvSrvUavDescriptorHeap.currentDescriptorIndex;

        m_cbvSrvUavDescriptorHeap.offset();

        setName(texture.texture.Get(), textureName);

        if (generateMipMaps)
        {
            generateMips(texture);
        }

        return texture;
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

    StructuredBuffer Engine::createStructuredBuffer(const std::byte* data, const uint32_t numberOfComponents, const uint32_t stride, const std::wstring_view bufferName)
    {
        StructuredBuffer structuredBuffer{};

        // Setup the resource desc for buffer creation.
        const D3D12_RESOURCE_DESC structuredBufferResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0u,
            .Width = numberOfComponents * stride,
            .Height = 1u,
            .DepthOrArraySize = 1u,
            .MipLevels = 1u,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {1u, 0u},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        structuredBuffer.buffer = createBuffer(structuredBufferResourceDesc, data, bufferName);

        // Create the shader resource view.
        const D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer =
                {
                    .FirstElement = 0u,

                    .NumElements = numberOfComponents,
                    .StructureByteStride = stride,
                },
        };

        m_device->CreateShaderResourceView(structuredBuffer.buffer.Get(), &shaderResourceViewDesc, m_cbvSrvUavDescriptorHeap.currentCpuDescriptorHandle);
        structuredBuffer.srvIndex = m_cbvSrvUavDescriptorHeap.currentDescriptorIndex;

        m_cbvSrvUavDescriptorHeap.offset();

        return structuredBuffer;
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

        std::vector<math::XMFLOAT3> positionData{};
        std::vector<math::XMFLOAT2> textureCoordData{};
        std::vector<math::XMFLOAT3> normalData{};

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

            positionData.reserve(positionAccesor.count);
            textureCoordData.reserve(textureCoordAccesor.count);
            normalData.reserve(normalAccesor.count);

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

                positionData.emplace_back(position);
                textureCoordData.emplace_back(textureCoord);
                normalData.emplace_back(normal);
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

        Mesh mesh{};
        mesh.indexBuffer = createIndexBuffer(reinterpret_cast<const std::byte*>(indices.data()), indexBufferSize, stringToWString(modelPath) + std::wstring(L" Index buffer"));

        mesh.positionBuffer = createStructuredBuffer(reinterpret_cast<const std::byte*>(positionData.data()),
                                                     static_cast<uint32_t>(positionData.size()),
                                                     sizeof(math::XMFLOAT3),
                                                     stringToWString(modelPath) + std::wstring(L" Position buffer"));

        mesh.textureCoordBuffer = createStructuredBuffer(reinterpret_cast<const std::byte*>(textureCoordData.data()),
                                                         static_cast<uint32_t>(textureCoordData.size()),
                                                         sizeof(math::XMFLOAT2),
                                                         stringToWString(modelPath) + std::wstring(L" Texture Coord buffer"));
        mesh.normalBuffer = createStructuredBuffer(reinterpret_cast<const std::byte*>(normalData.data()),
                                                   static_cast<uint32_t>(normalData.size()),
                                                   sizeof(math::XMFLOAT3),
                                                   stringToWString(modelPath) + std::wstring(L" Normal buffer"));

        return mesh;
    }
    void Engine::createPipeline(const std::wstring vertexShaderPath, const std::wstring pixelShaderPath, const std::wstring pipelineName)
    {
        // Setup shaders.
        const Shader vertexShader = ShaderCompiler::compile(ShaderTypes::Vertex, vertexShaderPath);
        const Shader pixelShader = ShaderCompiler::compile(ShaderTypes::Pixel, pixelShaderPath);

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

        // Setup depth stencil state.
        const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
            .StencilEnable = false,
        };

        const D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc = {
            .pRootSignature = m_bindlessRootSignature.Get(),
            .VS = vertexShaderByteCode,
            .PS = pixelShaderByteCode,
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT_MAX,
            .RasterizerState = defaultRasterizerDesc,
            .DepthStencilState = depthStencilDesc,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1u,
            .RTVFormats = DXGI_FORMAT_R8G8B8A8_UNORM,
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {1u, 0u},
            .NodeMask = 0u,
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        throwIfFailed(m_device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&m_graphicsPipelines[pipelineName.data()].pipelineState)));
        setName(m_graphicsPipelines[L"BasePipeline"].pipelineState.Get(), pipelineName.data() + std::wstring(L"Pipeline State"));
    }

    void Engine::generateMips(Texture& texture)
    {
        D3D12_RESOURCE_DESC sourceResourceDesc = texture.texture->GetDesc();
        if (sourceResourceDesc.MipLevels <= 1)
        {
            return;
        }

        // Start the mip generation process.
        for (uint32_t srcMipLevel = 0; srcMipLevel < sourceResourceDesc.MipLevels - 1u;)
        {
            uint64_t sourceWidth = sourceResourceDesc.Width >> srcMipLevel;
            uint64_t sourceHeight = sourceResourceDesc.Height >> srcMipLevel;

            // Destination width and height is half of that of source width and height.
            uint32_t destinationWidth = std::max<uint32_t>((uint32_t)sourceWidth >> 1u, 1u);
            uint32_t destinationHeight = std::max<uint32_t>((uint32_t)sourceHeight >> 1u, 1u);

            // Find the dimension type.
            DimensionType dimensionType{};
            if (sourceHeight % 2 == 0 && sourceWidth % 2 == 0)
            {
                dimensionType = DimensionType::WidthHeightEven;
            }
            else if (sourceHeight % 2 != 0 && sourceWidth % 2 == 0)
            {
                dimensionType = DimensionType::WidthEvenHeightOdd;
            }
            else if (sourceHeight % 2 == 0 && sourceWidth % 2 != 0)
            {
                dimensionType = DimensionType::WidthOddHeightEven;
            }
            else
            {
                dimensionType = DimensionType::WidthHeightOdd;
            }

            // At a single compute shader dispatch, we can generate atmost 4 mip maps.
            // The code below checks for in this loop iteration, how many levels can we compute, so as to have subsequent mip level dimension
            // be exactly half : exactly 50 % decrease in mip dimension.
            // i.e number of times mip can be halved until we reach a mip level where one dimension is odd.
            // If dimension is odd, texture needs to be sampled multiple times, which will be handled in a new dispatch.
            DWORD mipCount{};
            // Value of temp not required.
            _BitScanForward64(&mipCount, (destinationWidth == 1u ? destinationHeight : destinationWidth) | (destinationHeight == 1u ? destinationWidth : destinationHeight));
            mipCount = std::min<uint32_t>(4, mipCount + 1);
            mipCount = (srcMipLevel + mipCount) >= sourceResourceDesc.MipLevels ? sourceResourceDesc.MipLevels - srcMipLevel - 1u : mipCount;

            // NOTE : UAV's have a limited set of formats they can use.
            // Refer : https://docs.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads.
            std::array<uint32_t, 4u> mipUavs{};
            for (uint32_t uav : std::views::iota(0u, static_cast<uint32_t>(mipCount)))
            {
                const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
                    .Format = getNonSRGBFormat(sourceResourceDesc.Format),
                    .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                    .Texture2D{
                        .MipSlice = uav + 1 + srcMipLevel,
                        .PlaneSlice = 0u,
                    },
                };

                //  Reading from a SRV/UAV mapped to a null resource will return black and writing to a UAV mapped to a null resource will have no effect (from 3DGEP).
                m_device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, m_cbvSrvUavDescriptorHeap.currentCpuDescriptorHandle);

                mipUavs[uav] = m_cbvSrvUavDescriptorHeap.currentDescriptorIndex;
                m_cbvSrvUavDescriptorHeap.offset();
            }
            m_mipGenBuffer.data.dimensionType = dimensionType;
            m_mipGenBuffer.data.isSrgb = isTextureSRGB(sourceResourceDesc.Format);
            m_mipGenBuffer.data.texelSize = {1.0f / destinationWidth, 1.0f / destinationHeight};
            m_mipGenBuffer.data.sourceMipLevel = srcMipLevel;
            m_mipGenBuffer.data.numberOfMipLevels = mipCount;

            m_mipGenBuffer.update();

            struct RenderResources
            {
                uint32_t mipGenBufferIndex;
                uint32_t sourceTextureIndex;
                uint32_t outputMip1Index;
                uint32_t outputMip2Index;
                uint32_t outputMip3Index;
                uint32_t outputMip4Index;
            };

            const RenderResources renderResources = {
                .mipGenBufferIndex = m_mipGenBuffer.cbvIndex,
                .sourceTextureIndex = texture.srvIndex,
                .outputMip1Index = mipUavs[0],
                .outputMip2Index = mipUavs[1],
                .outputMip3Index = mipUavs[2],
                .outputMip4Index = mipUavs[3],
            };

            const std::array<ID3D12DescriptorHeap*, 1u> shaderVisibleDescriptorHeaps{m_cbvSrvUavDescriptorHeap.descriptorHeap.Get()};
            m_computeCommandList->SetDescriptorHeaps(1u, shaderVisibleDescriptorHeaps.data());
            m_computeCommandList->SetComputeRootSignature(m_bindlessRootSignature.Get());
            m_computeCommandList->SetPipelineState(m_mipMapGenerationPipeline.pipelineState.Get());

            m_computeCommandList->SetComputeRoot32BitConstants(0u, 64, &renderResources, 0u);

            m_computeCommandList->Dispatch(std::max<uint32_t>((uint32_t)std::ceil(destinationWidth / 8.0f), 1u),
                                           std::max<uint32_t>((uint32_t)std::ceil(destinationHeight / 8.0f), 1u),
                                           1);

            executeComputeCommands();

            srcMipLevel += static_cast<uint32_t>(mipCount);
        };
    }
}
