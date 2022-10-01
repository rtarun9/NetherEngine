#include "Pch.hpp"

#include "DescriptorHeap.hpp"

namespace nether::graphics
{
	void DescriptorHeap::Init(ID3D12Device5* const device, const D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, const uint32_t descriptorCount, const std::wstring_view descriptorHeapName)
	{
		// Create descriptor heap and get descriptor size.

		// Set descriptorHeapFlag and find out if descriptor is shader visible.
		const D3D12_DESCRIPTOR_HEAP_FLAGS descriptorHeapFlags = [&]() 
		{
			switch (descriptorHeapType)
			{
			case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
			case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
			{
				mIsShaderVisible = true;
				return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			}break;

			default:
			{
				mIsShaderVisible = false;
				return D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			}break;
			}
		}();

		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc
		{
			.Type = descriptorHeapType,
			.NumDescriptors = descriptorCount,
			.Flags = descriptorHeapFlags,
			.NodeMask = 0u
		};

		ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&mDescriptorHeap)));
		SetName(mDescriptorHeap.Get(), descriptorHeapName.data());

		mDescriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(descriptorHeapType);

		// Setup descriptor handle from heap start.
		const D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandleForHeapStart = [&]()
		{
			if (mIsShaderVisible)
			{
				return mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			}
			else
			{
				return D3D12_GPU_DESCRIPTOR_HANDLE{};
			}
		}();

		mDescriptorHandleForHeapStart =
		{
			.cpuDescriptorHandle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			.gpuDescriptorHandle = gpuDescriptorHandleForHeapStart,
			.index = 0u
		};

		mCurrentDescriptorHandle = mDescriptorHandleForHeapStart;
	}

	DescriptorHandle DescriptorHeap::GetDescriptorHandleFromIndex(const uint32_t index) const
	{
		return DescriptorHandle
		{
			.cpuDescriptorHandle = mDescriptorHandleForHeapStart.cpuDescriptorHandle.ptr + index * mDescriptorHandleIncrementSize,
			.gpuDescriptorHandle = mDescriptorHandleForHeapStart.gpuDescriptorHandle.ptr + index * mDescriptorHandleIncrementSize,
			.index = index
		};
	}

	void DescriptorHeap::OffsetCurrentDescriptorHandle()
	{
		mCurrentDescriptorHandle.cpuDescriptorHandle.ptr += mDescriptorHandleIncrementSize;
		mCurrentDescriptorHandle.index++;

		if (mIsShaderVisible)
		{
			mCurrentDescriptorHandle.gpuDescriptorHandle.ptr += mDescriptorHandleIncrementSize;
		}
	}

	void DescriptorHeap::OffsetDescriptorHandle(DescriptorHandle& descriptorHandle) const
	{
		descriptorHandle.cpuDescriptorHandle.ptr += mDescriptorHandleIncrementSize;
		descriptorHandle.index++;

		if (mIsShaderVisible)
		{
			descriptorHandle.gpuDescriptorHandle.ptr += mDescriptorHandleIncrementSize;
		}
	}
}
