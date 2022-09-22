#pragma once

#include "Rendering/Resources.hpp"

struct TransformComponent
{
	DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT3 rotate{};
	DirectX::XMFLOAT3 translate{};
};

struct alignas(256) MVPBuffer
{
	DirectX::XMMATRIX modelMatrix{};
	DirectX::XMMATRIX inverseModelMatrix{};
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
		rendering::ConstantBuffer* GetTransformConstantBuffer() const { return mTransformBuffer.get(); }
		TransformComponent& GetTransformComponent() { return mTransformComponenet; }

	public:
		std::vector<rendering::VertexBuffer> mVertexBuffers{};
		std::vector<rendering::IndexBuffer> mIndexBuffers{};
		std::vector<uint32_t> mMaterialIndices{};

		uint32_t mMeshCount{};

		// Texture paths are relative to this.
		std::wstring mModelDirectory{};

		std::vector<rendering::Texture> mAlbedoTextures{};
		std::unique_ptr<rendering::ConstantBuffer> mTransformBuffer{};

		TransformComponent mTransformComponenet{};

	private:
		// In the case any texture is missing, use this prototype texture.
		static inline std::unique_ptr<rendering::Texture> sDefaultTexture;
	};
}