#include "Pch.hpp"

#include "CommandQueue.hpp"

#include "Utils.hpp"

namespace nether
{
	CommandQueue::CommandQueue(ID3D12Device5* const device, const D3D12_COMMAND_LIST_TYPE commandListType, const std::wstring_view commandQueueName)
	{
		mCommandListType = commandListType;

		// Create direct command queue (execution port of GPU -> has methods for submitting command list and their execution).
		const D3D12_COMMAND_QUEUE_DESC commandQueueDesc
		{
			.Type = commandListType,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0u,
		};

		utils::ThrowIfFailed(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)));
		utils::SetName(mCommandQueue.Get(), commandQueueName.data());

		// Create command allocators (one for each frame).
		for (const uint32_t frameIndex : std::views::iota(0u, NUMBER_OF_FRAMES))
		{
			device->CreateCommandAllocator(mCommandListType, IID_PPV_ARGS(&mCommandAllocators[frameIndex]));
			utils::SetName(mCommandAllocators[frameIndex].Get(), commandQueueName.data());
		}

		// Create synchronization primitives.
		utils::ThrowIfFailed(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		utils::SetName(mFence.Get(), L"Direct Command Queue Fence");

		mFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!mFenceEvent)
		{
			utils::ErrorMessage(L"Failed to create fence event.");
		}
	}

	uint64_t CommandQueue::Signal()
	{
		const uint64_t fenceValueToSignal = ++mFenceValue;
		utils::ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), fenceValueToSignal));

		return fenceValueToSignal;
	}

	void CommandQueue::WaitForFenceValue(const uint64_t fenceValue)
	{
		if (mFence->GetCompletedValue() < fenceValue)
		{
			utils::ThrowIfFailed(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
			::WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
		}
	}

	void CommandQueue::Flush(const uint32_t frameIndex)
	{
		const uint64_t fenceValue = Signal();
		WaitForFenceValue(fenceValue);
		mFrameFenceValues[frameIndex] = fenceValue;
	}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList(ID3D12Device5* const device, const uint32_t frameIndex)
	{
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList{};

		if (!mCommandLists.empty())
		{
			commandList = mCommandLists.front();
			mCommandLists.pop();
		}
		else
		{
			// Create new command list.
			utils::ThrowIfFailed(device->CreateCommandList1(0u, mCommandListType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList)));
		}

		utils::ThrowIfFailed(commandList->Reset(mCommandAllocators[frameIndex].Get(), nullptr));

		return commandList;
	}

	void CommandQueue::ExecuteCommandLists(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, const uint32_t frameIndex)
	{
		utils::ThrowIfFailed(commandList->Close());
		const std::array<ID3D12CommandList* const, 1u> commandLists
		{
			commandList.Get()
		};

		mCommandQueue->ExecuteCommandLists(1u, commandLists.data());
		mFrameFenceValues[frameIndex] = Signal();

		mCommandLists.push(std::move(commandList));
	}

}