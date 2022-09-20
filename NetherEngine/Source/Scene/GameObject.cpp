#include "Pch.hpp"

#include "GameObject.hpp"

#include "Loaders/ModelLoader.hpp"
#include "Rendering/Device.hpp"
#include "Rendering/Resources.hpp"

namespace nether::scene
{
	GameObject::GameObject(rendering::Device* const device, const std::wstring_view modelPath, const std::wstring_view objectName)
	{
		const std::wstring transformBufferName = objectName.data() + std::wstring(L" Constant Buffer");
		const std::wstring vertexBufferName = objectName.data() + std::wstring(L" Vertex Buffer");
		const std::wstring indexBufferName = objectName.data() + std::wstring(L" Index Buffer");
		
		rendering::ConstantBufferCreationDesc transformBufferCreationDesc
		{
			.bufferSize = sizeof(MVPBuffer)
		};

		mTransformBuffer = std::make_unique<rendering::ConstantBuffer>(device->CreateConstantBuffer(transformBufferCreationDesc, transformBufferName.data()));
		loaders::ModelLoader modelLoader(modelPath);

		const size_t meshesCount = modelLoader.mMeshes.size();
		
		for (size_t i : std::views::iota(0u, meshesCount))
		{
			const rendering::VertexBufferCreationDesc vertexBufferCreationDesc
			{
				.numberOfElements = static_cast<uint32_t>(modelLoader.mMeshes[i].vertices.size()),
				.stride = sizeof(Vertex)
			};

			const rendering::IndexBufferCreationDesc indexBufferCreationDesc
			{
				.bufferSize = static_cast<uint32_t>(modelLoader.mMeshes[i].indices.size() * sizeof(uint32_t)),
				.indicesCount = static_cast<uint32_t>(modelLoader.mMeshes[i].indices.size()),
				.format = DXGI_FORMAT_R32_UINT,
			};

			rendering::VertexBuffer modelVertexBuffer = device->CreateVertexBuffer(vertexBufferCreationDesc, modelLoader.mMeshes[i].vertices.data(), vertexBufferName.data());
			rendering::IndexBuffer modelIndexBuffer = device->CreateIndexBuffer(indexBufferCreationDesc, modelLoader.mMeshes[i].indices.data(), indexBufferName.data());

			mVertexBuffers.push_back(std::move(modelVertexBuffer));
			mIndexBuffers.push_back(std::move(modelIndexBuffer));
		}
	}
}