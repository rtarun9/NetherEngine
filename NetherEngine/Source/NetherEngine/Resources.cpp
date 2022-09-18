#include "Pch.hpp"

#include "Resources.hpp"

namespace nether
{
	void ConstantBuffer::Update(void* data)
	{
		std::memcpy((void*)mappedBufferPointer, data, bufferSize);
	}
}