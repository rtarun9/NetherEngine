#include "Pch.hpp"

#include "DescriptorHeap.hpp"

#include "GraphicsCommon.hpp"
#include "Common/Utils.hpp"

namespace nether::rendering
{
	DescriptorHeap::DescriptorHeap(ID3D12Device5* const device, const D3D12_DESCRIPTOR_HEAP_TYPE& descriptorHeapType, uint32_t numberOfDescriptors, const std::wstring_view descriptorName)
	{
		mIsShaderVisible = descriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		// Create descriptor heap.
		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc
		{
			.Type = descriptorHeapType,
			.NumDescriptors = numberOfDescriptors,
			.Flags = mIsShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0u
		};

		utils::ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&mDescriptorHeap)));
		SetName(mDescriptorHeap.Get(), descriptorName.data());

		mDescriptorHandleSize = device->GetDescriptorHandleIncrementSize(descriptorHeapType);

		// Get descriptor handle's from descriptor heap start.
		mDescriptorHandleFromHeapStart.cpuDescriptorHandle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		if (mIsShaderVisible)
		{
			mDescriptorHandleFromHeapStart.gpuDescriptorHandle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		mCurrentDescriptorHandle = mDescriptorHandleFromHeapStart;
	}

	void DescriptorHeap::OffsetCurrentDescriptor()
	{
		mCurrentDescriptorHandle.cpuDescriptorHandle.ptr += mDescriptorHandleSize;

		if (mIsShaderVisible)
		{
			mCurrentDescriptorHandle.gpuDescriptorHandle.ptr += mDescriptorHandleSize;
		}
	}
}