#pragma once

struct Uint2
{
    uint32_t x{};
    uint32_t y{};

    auto operator<=>(const Uint2& other) const = default;
};

struct DescriptorHeap
{
    Comptr<ID3D12DescriptorHeap> descriptorHeap{};
    uint32_t descriptorSize{};

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandleFromHeapStart{};
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandleFromHeapStart{};

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentCpuDescriptorHandle{};
    CD3DX12_GPU_DESCRIPTOR_HANDLE currentGpuDescriptorHandle{};

    uint32_t currentDescriptorIndex{};

    void init(ID3D12Device5* device, const D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t descriptorCount, const std::wstring_view descriptorName)
    {
        const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {
            .Type = heapType,
            .NumDescriptors = descriptorCount,
            .Flags = ((heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) || (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)) ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                                                                                : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };

        throwIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

        descriptorSize = device->GetDescriptorHandleIncrementSize(heapType);

        cpuDescriptorHandleFromHeapStart = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        currentCpuDescriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();

        if (descriptorHeapDesc.Flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        {
            gpuDescriptorHandleFromHeapStart = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
            currentGpuDescriptorHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
        }
    }

    void offset()
    {
        currentDescriptorIndex++;

        currentCpuDescriptorHandle.Offset(descriptorSize);
        currentGpuDescriptorHandle.Offset(descriptorSize);
    }

    void offset(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuDescriptorHandle) { cpuDescriptorHandle.Offset(descriptorSize); }
    void offset(CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuDescriptorHandle) { gpuDescriptorHandle.Offset(descriptorSize); }

    CD3DX12_CPU_DESCRIPTOR_HANDLE getCpuDescriptorHandleAtIndex(const uint32_t index)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle = cpuDescriptorHandleFromHeapStart;
        return descriptorHandle.Offset(index, descriptorSize);
    }
};

// Assumptions : All root parameters will be inline descriptors for now.
struct Shader
{
    Comptr<IDxcBlob> shaderBlob{};
};

struct GraphicsPipeline
{
    Comptr<ID3D12RootSignature> rootSignature{};
    Comptr<ID3D12PipelineState> pipelineState{};
};

// For now the compute pipeline does not have any sort of reflection going on.
struct ComputePipeline
{
    Comptr<ID3D12RootSignature> rootSignature{};
    Comptr<ID3D12PipelineState> pipelineState{};

    Shader computeShader{};
};

struct VertexBuffer
{
    uint32_t verticesCount{};
    Comptr<ID3D12Resource> buffer{};
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
};

struct IndexBuffer
{
    Comptr<ID3D12Resource> buffer{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
};

struct StructuredBuffer
{
    Comptr<ID3D12Resource> buffer{};
    uint32_t srvIndex{};
};

struct Mesh
{
    StructuredBuffer positionBuffer{};
    StructuredBuffer textureCoordBuffer{};
    StructuredBuffer normalBuffer{};

    IndexBuffer indexBuffer{};
};

struct Texture
{
    Comptr<ID3D12Resource> texture{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle{};
    uint32_t srvIndex{};
};

template <typename T> struct ConstantBuffer
{
    Comptr<ID3D12Resource> buffer{};
    T data{};
    uint8_t* bufferPointer{};
    uint32_t srvIndex{};

    void update()
    {
        // Set null read range, as we don't intend on reading from this resource on the CPU.
        constexpr D3D12_RANGE readRange = {
            .Begin = 0u,
            .End = 0u,
        };

        const uint32_t bufferSize = static_cast<uint32_t>(sizeof(T));

        throwIfFailed(buffer->Map(0u, &readRange, reinterpret_cast<void**>(&bufferPointer)));
        std::memcpy(bufferPointer, &data, bufferSize);
        buffer->Unmap(0u, nullptr);
    }
};

struct alignas(256) TransformData
{
    math::XMMATRIX modelMatrix{};
};

struct Renderable
{
    Mesh* mesh{};
    GraphicsPipeline* graphicsPipeline{};
    ConstantBuffer<TransformData> transformBuffer{};
};

struct alignas(256) SceneData
{
    math::XMMATRIX viewProjectionMatrix{};
};

struct FrameResources
{
    Comptr<ID3D12CommandAllocator> commandAllocator{};
    Comptr<ID3D12GraphicsCommandList2> commandList{};

    uint64_t fenceValue{};

    ConstantBuffer<SceneData> sceneBuffer{};
};
