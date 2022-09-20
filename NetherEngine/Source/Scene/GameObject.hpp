#pragma once

#include "Rendering/Resources.hpp"

struct alignas(256) MVPBuffer
{
	DirectX::XMMATRIX modelMatrix{};
	DirectX::XMMATRIX viewProjectionMatrix{};
};

// Forward declaration's.
namespace nether::rendering
{
	class Device;
	class ConstantBuffer;
}

namespace nether::scene
{
	class GameObject
	{
	public:
		GameObject(rendering::Device* const device, const std::wstring_view modelPath, const std::wstring_view objectName);

		// Getters.
		size_t GetIndicesCount() const { return mIndexBuffers.size(); }
		rendering::ConstantBuffer* GetTransformConstantBuffer() const { return mTransformBuffer.get(); }

	public:
		std::vector<rendering::VertexBuffer> mVertexBuffers{};
		std::vector<rendering::IndexBuffer> mIndexBuffers{};

		std::unique_ptr<rendering::ConstantBuffer> mTransformBuffer{};
	};
}