#pragma once

#include "GraphicsCommon.hpp"

namespace nether::graphics
{
	// CommandQueue abstraction, which has a array of command allocators (one per frame) and holds the synchronization primitives and related function's.
	// There is a queue of command list's, where we create a new list if none are present and reuse previously created ones.
	class CommandQueue
	{
	public:
		CommandQueue() = default;
		~CommandQueue() = default;

		CommandQueue(const CommandQueue& other) = delete;
		CommandQueue& operator=(const CommandQueue& other) = delete;

		CommandQueue(CommandQueue&& other) = delete;
		CommandQueue& operator=(CommandQueue&& other) = delete;

		void Init(ID3D12Device5* const device, const D3D12_COMMAND_LIST_TYPE commandListType, const std::wstring_view commandQueueName);

		// Accessors and modifiers.
		ID3D12CommandQueue* GetCommandQueue() const { return mCommandQueue.Get(); }
		ID3D12CommandAllocator* GetCommandAllocator(const uint32_t frameIndex) const { return mCommandAllocators[frameIndex].Get(); }
		uint64_t GetFrameFenceValue(const uint32_t frameIndex) const { return mFrameFenceValues[frameIndex]; }

		// Create a command list with backing command allocator as mCommandAllocator[frameIndex].
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList(const uint32_t frameIndex);

		uint64_t Signal(const uint64_t frameFenceValue);
		void ExecuteCommandLists(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, const uint32_t frameIndex);
		bool IsFenceComplete(const uint64_t frameFenceValue);
		void WaitForFenceValue(const uint64_t frameFenceValue);

		void Flush(const uint32_t frameIndex);

	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue{};
		std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, BACK_BUFFER_COUNT> mCommandAllocators{};

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
		std::array<uint64_t, BACK_BUFFER_COUNT> mFrameFenceValues{0u};

		std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> mCommandLists{};

		D3D12_COMMAND_LIST_TYPE mCommandListType{};

		// Creation of command list happens via the ID3D12Device. So, this class holds a shared reference to the ID3D12Device.
		// Using ComPtr as it works similar to a shared pointer.
		Microsoft::WRL::ComPtr<ID3D12Device5> mDevice{};
	};
}
