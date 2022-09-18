#pragma once

namespace nether
{
	struct Descriptor
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle{};
	};

	class DescriptorHeap
	{
	public:
		DescriptorHeap(ID3D12Device5* const device, const D3D12_DESCRIPTOR_HEAP_TYPE& descriptorHeapType, uint32_t numberOfDescriptors, const std::wstring_view descriptorName);

		void OffsetCurrentDescriptor();

		Descriptor GetDescriptorHandleFromHeapStart() const { return mDescriptorHandleFromHeapStart; }
		uint32_t GetDescriptorHandleSize() const { return mDescriptorHandleSize; }

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap{};
		uint32_t mDescriptorHandleSize{};
		bool mIsShaderVisible{};

		Descriptor mDescriptorHandleFromHeapStart{};
		Descriptor mCurrentDescriptorHandle{};
	};
}