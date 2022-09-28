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
		for (uint32_t index : std::views::iota(0u, BACK_BUFFER_COUNT))
		{
			ThrowIfFailed(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&mCommandAllocators[index])));
			const std::wstring commandAllocatorName = commandQueueName.data() + std::to_wstring(index);

			SetName(mCommandAllocators[index].Get(), commandAllocatorName);
		}

		// Create sync primitives.
		ThrowIfFailed(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		const std::wstring fenceName = commandQueueName.data() + std::wstring(L" Fence");
		SetName(mFence.Get(), fenceName);
		
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
			// Create HANDLE that will be used to block CPU thread until GPU has completed execution.
			HANDLE fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (!fenceEvent)
			{
				ErrorMessage(L"Failed to create fence event");
			}

			// Specify that the fence event will be fired when the fence reaches the specified value.
			ThrowIfFailed(mFence->SetEventOnCompletion(frameFenceValue, fenceEvent));

			// Wait until that event has been triggered.

			assert(fenceEvent && "Fence Event is NULL");
			::WaitForSingleObject(fenceEvent, INFINITE);
			CloseHandle(fenceEvent);
		}
	}

	void CommandQueue::Flush(const uint32_t frameIndex)
	{
		mFrameFenceValues[frameIndex] = Signal(mFrameFenceValues[frameIndex]);
		WaitForFenceValue(mFrameFenceValues[frameIndex]);
	}
}