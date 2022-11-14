#pragma once

struct Uint2
{
    uint32_t x{};
    uint32_t y{};

    auto operator<=>(const Uint2& other) const = default;
};

struct Pipeline
{
    Comptr<ID3D12RootSignature> rootSignature{};
    Comptr<ID3D12PipelineState> pipelineState{};
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

struct Mesh
{
    VertexBuffer vertexBuffer{};
    IndexBuffer indexBuffer{};
};

struct FrameResources
{
    Comptr<ID3D12CommandAllocator> commandAllocator{};
    Comptr<ID3D12GraphicsCommandList2> commandList{};

    uint64_t fenceValue{};
};

template <typename T> struct ConstantBuffer
{
    Comptr<ID3D12Resource> buffer{};
    T data{};
    uint8_t* bufferPointer{};

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