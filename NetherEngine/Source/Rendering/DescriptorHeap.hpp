#pragma once

namespace nether::rendering
{
	// Holds CPU and GPU descriptor handle.
	struct Descriptor
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle{};
	};

	// Abstraction for descriptor heap : Use the CurrentDescriptor for creating descriptor heaps easily.
	class DescriptorHeap
	{
	public:
		DescriptorHeap(ID3D12Device5* const device, const D3D12_DESCRIPTOR_HEAP_TYPE& descriptorHeapType, uint32_t numberOfDescriptors, const std::wstring_view descriptorName);

		// Getters.
		ID3D12DescriptorHeap* GetDescriptorHeap() const { return mDescriptorHeap.Get(); }
		uint32_t GetDescriptorHandleSize() const { return mDescriptorHandleSize; }
		Descriptor GetDescriptorHandleFromHeapStart() const { return mDescriptorHandleFromHeapStart; }
		Descriptor GetCurrentDescriptorHeap() const { return mCurrentDescriptorHandle; }

		// Core function's.
		void OffsetCurrentDescriptor();

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap{};
		uint32_t mDescriptorHandleSize{};
		bool mIsShaderVisible{};

		Descriptor mDescriptorHandleFromHeapStart{};
		Descriptor mCurrentDescriptorHandle{};
	};
}