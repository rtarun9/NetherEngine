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

struct Mesh
{
    Comptr<ID3D12Resource> vertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
};

struct FrameResources
{
    Comptr<ID3D12CommandAllocator> commandAllocator{};
    Comptr<ID3D12GraphicsCommandList2> commandList{};

    uint64_t fenceValue{};
};