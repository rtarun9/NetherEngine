#pragma once

#include "Common.hpp"

namespace nether
{
	class CommandQueue
	{
	public:
		CommandQueue(ID3D12Device5* const device, const D3D12_COMMAND_LIST_TYPE commandListType, const std::wstring_view commandQueueName);

		uint64_t Signal();
		void WaitForFenceValue(const uint64_t fenceValue);
		void Flush(const uint32_t frameIndex);

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList(ID3D12Device5* const device, const uint32_t frameIndex);
		void ExecuteCommandLists(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, const uint32_t frameIndex);

		// Note : TEMP.
	public:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue{};

		std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, NUMBER_OF_FRAMES> mCommandAllocators{};
		std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> mCommandLists{};

		D3D12_COMMAND_LIST_TYPE mCommandListType{};

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
		HANDLE mFenceEvent{};
		uint64_t mFenceValue{};
		std::array<uint64_t, NUMBER_OF_FRAMES> mFrameFenceValues{};
	};
}