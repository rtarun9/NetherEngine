#pragma once

namespace nether::graphics
{
	// Store the CPU and GPU descriptor handle together.
	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle{};
		uint32_t index{};
	};

	// Abstraction for descriptor heaps : used to encompass memory allocation's used for storing descriptors.
	class DescriptorHeap
	{
	public:
		DescriptorHeap() = default;
		~DescriptorHeap() = default;

		void Init(ID3D12Device5* const device, const D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, const uint32_t descriptorCount, const std::wstring_view descriptorHeapName);

		DescriptorHeap(const DescriptorHeap& other) = delete;
		DescriptorHeap& operator=(const DescriptorHeap& other) = delete;

		DescriptorHeap(DescriptorHeap&& other) = delete;
		DescriptorHeap& operator=(DescriptorHeap&& other) = delete;

		// Accessors and modifiers.
		ID3D12DescriptorHeap* GetDescriptorHeap() const { return mDescriptorHeap.Get(); }
		DescriptorHandle GetCurrentDescriptorHandle() const { return mCurrentDescriptorHandle; }
		DescriptorHandle GetDescriptorHandleForHeapStart() const { return mDescriptorHandleForHeapStart; }
		DescriptorHandle GetDescriptorHandleFromIndex(const uint32_t index) const;
		uint32_t GetDescriptorHandleSize() const { return mDescriptorHandleIncrementSize; }

		void OffsetCurrentDescriptorHandle();
		void OffsetDescriptorHandle(DescriptorHandle& descriptorHandle) const;

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap{};
		uint32_t mDescriptorHandleIncrementSize{};

		DescriptorHandle mDescriptorHandleForHeapStart{};
		DescriptorHandle mCurrentDescriptorHandle{};

		bool mIsShaderVisible{false};
	};
}
