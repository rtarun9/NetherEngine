#include "Pch.hpp"

#include "CommandQueue.hpp"

namespace nether::graphics
{
	void CommandQueue::Init(ID3D12Device5* const device, const D3D12_COMMAND_LIST_TYPE commandListType, const std::wstring_view commandQueueName)
	{
		// Create command queue.
		const D3D12_COMMAND_QUEUE_DESC commandQueueDesc
		{
			.Type = commandListType,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0u,
		};

		ThrowIfFailed(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)));
		SetName(mCommandQueue.Get(), commandQueueName);

		// Create command allocators.
		for (const uint32_t index : std::views::iota(0u, FRAMES_IN_FLIGHT))
		{
			ThrowIfFailed(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&mCommandAllocators[index])));

			const std::wstring commandAllocatorName = commandQueueName.data() + std::to_wstring(index);
			SetName(mCommandAllocators[index].Get(), commandAllocatorName);
		}

		// Create sync primitives.
		ThrowIfFailed(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		const std::wstring fenceName = commandQueueName.data() + std::wstring(L" Fence");
		SetName(mFence.Get(), fenceName);
		
		mFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

		mDevice = device;
		mCommandListType = commandListType;
	}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList(const uint32_t frameIndex)
	{
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList{};

		if (mCommandLists.empty())
		{
			mDevice->CreateCommandList(0u, mCommandListType, mCommandAllocators[frameIndex].Get(), nullptr, IID_PPV_ARGS(&commandList));
		}
		else
		{
			commandList = mCommandLists.front();
			mCommandLists.pop();
			ThrowIfFailed(commandList->Reset(mCommandAllocators[frameIndex].Get(), nullptr));
		}

		return commandList;
	}

	uint64_t CommandQueue::Signal(const uint64_t frameFenceValue)
	{
		const uint64_t frameFenceValueToSignal = frameFenceValue + 1u;
		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), frameFenceValueToSignal));

		return frameFenceValueToSignal;
	}

	void CommandQueue::ExecuteCommandLists(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, const uint32_t frameIndex)
	{
		ThrowIfFailed(commandList->Close());

		const std::array<ID3D12CommandList* const, 1u> commandLists
		{
			commandList.Get()
		};

		mCommandQueue->ExecuteCommandLists(1u, commandLists.data());
		mFrameFenceValues[frameIndex] = Signal(mFrameFenceValues[frameIndex]);

		mCommandLists.push(std::move(commandList));
	}

	bool CommandQueue::IsFenceComplete(const uint64_t frameFenceValue)
	{
		return mFence->GetCompletedValue() >= frameFenceValue;
	}

	void CommandQueue::WaitForFenceValue(const uint64_t frameFenceValue)
	{
		if (mFence->GetCompletedValue() < frameFenceValue)
		{
			// Specify that the fence event will be fired when the fence reaches the specified value.
			ThrowIfFailed(mFence->SetEventOnCompletion(frameFenceValue, mFenceEvent));

			// Wait until that event has been triggered.
			::WaitForSingleObject(mFenceEvent, INFINITE);
		}
	}

	void CommandQueue::WaitForFenceValueAtFrameIndex(const uint32_t frameIndex)
	{
		WaitForFenceValue(mFrameFenceValues[frameIndex]);
	}

	void CommandQueue::Flush(const uint32_t frameIndex)
	{
		const uint64_t frameFenceValue = mFrameFenceValues[frameIndex] + 1u;
		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), frameFenceValue));

		// Specify that the fence event will be fired when the fence reaches the specified value.
		ThrowIfFailed(mFence->SetEventOnCompletion(frameFenceValue, mFenceEvent));

		// Wait until that event has been triggered.
		::WaitForSingleObject(mFenceEvent, INFINITE);

		// When a command queue is flushed, all pending GPU commands have been executed.
		// Due to this, it makes sense to 'soft reset' the frame fence values, by basically making them all equal to frameFenceValue.
		for (const uint32_t index : std::views::iota(0u, FRAMES_IN_FLIGHT))
		{
			mFrameFenceValues[index] = frameFenceValue;
		}
	}
}