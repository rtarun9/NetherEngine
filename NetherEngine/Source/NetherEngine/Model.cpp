#include "Model.hpp"

#include "Utils.hpp"
#include "DataTypes.hpp"
#include "Engine.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE 
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace nether
{
	Model::Model(const std::wstring_view modelAssetPath, Engine* engine)
	{
		// Use tinyobj loader to load the model.

		const std::string modelPathStr = utils::WStringToString(modelAssetPath);

		std::string warning{};
		std::string error{};

		tinygltf::TinyGLTF context{};

		tinygltf::Model model{};

		if (!context.LoadASCIIFromFile(&model, &error, &warning, modelPathStr))
		{
			if (!error.empty())
			{
				utils::ErrorMessage(utils::StringToWString(error));
			}

			if (!warning.empty())
			{
				utils::ErrorMessage(utils::StringToWString(warning));
			}
		}

		// Build meshes.
		const tinygltf::Scene& scene = model.scenes[model.defaultScene];

		tinygltf::Node& node = model.nodes[0u];
		node.mesh = std::max<int32_t>(0u, node.mesh);

		const tinygltf::Mesh& nodeMesh = model.meshes[node.mesh];
		for (size_t i = 0; i < nodeMesh.primitives.size(); ++i)
		{
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};

			// Reference used : https://github.com/mateeeeeee/Adria-DX12/blob/fc98468095bf5688a186ca84d94990ccd2f459b0/Adria/Rendering/EntityLoader.cpp.

			// Get Accessors, buffer view and buffer for each attribute (position, textureCoord, normal).
			tinygltf::Primitive primitive = nodeMesh.primitives[i];
			const tinygltf::Accessor& indexAccesor = model.accessors[primitive.indices];

			// Position data.
			const tinygltf::Accessor& positionAccesor = model.accessors[primitive.attributes["POSITION"]];
			const tinygltf::BufferView& positionBufferView = model.bufferViews[positionAccesor.bufferView];
			const tinygltf::Buffer& positionBuffer = model.buffers[positionBufferView.buffer];

			const int positionByteStride = positionAccesor.ByteStride(positionBufferView);
			uint8_t const* const positions = &positionBuffer.data[positionBufferView.byteOffset + positionAccesor.byteOffset];

			// TextureCoord data.
			const tinygltf::Accessor& textureCoordAccesor = model.accessors[primitive.attributes["TEXCOORD_0"]];
			const tinygltf::BufferView& textureCoordBufferView = model.bufferViews[textureCoordAccesor.bufferView];
			const tinygltf::Buffer& textureCoordBuffer = model.buffers[textureCoordBufferView.buffer];
			const int textureCoordBufferStride = textureCoordAccesor.ByteStride(textureCoordBufferView);
			uint8_t const* const texcoords = &textureCoordBuffer.data[textureCoordBufferView.byteOffset + textureCoordAccesor.byteOffset];

			// Normal data.
			const tinygltf::Accessor& normalAccesor = model.accessors[primitive.attributes["NORMAL"]];
			const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccesor.bufferView];
			const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
			const int normalByteStride = normalAccesor.ByteStride(normalBufferView);
			uint8_t const* const normals = &normalBuffer.data[normalBufferView.byteOffset + normalAccesor.byteOffset];

			// Fill in the vertices's array.
			for (size_t i : std::views::iota(0u, positionAccesor.count))
			{
				const DirectX::XMFLOAT3 position
				{
					 (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[0],
					 (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[1],
					 (reinterpret_cast<float const*>(positions + (i * positionByteStride)))[2]
				};

				const DirectX::XMFLOAT2 textureCoord
				{
					(reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[0],
					(reinterpret_cast<float const*>(texcoords + (i * textureCoordBufferStride)))[1],
				};

				const DirectX::XMFLOAT3 normal
				{
					(reinterpret_cast<float const*>(normals + (i * normalByteStride)))[0],
					(reinterpret_cast<float const*>(normals + (i * normalByteStride)))[1],
					(reinterpret_cast<float const*>(normals + (i * normalByteStride)))[2],
				};

				vertices.emplace_back(Vertex{ position, textureCoord, normal });
			}

			// Get the index buffer data.
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccesor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
			const int indexByteStride = indexAccesor.ByteStride(indexBufferView);
			uint8_t const* const indexes = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccesor.byteOffset;

			// Fill indices array.
			for (size_t i : std::views::iota(0u, indexAccesor.count))
			{
				if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					indices.push_back(static_cast<uint32_t>((reinterpret_cast<uint16_t const*>(indexes + (i * indexByteStride)))[0]));
				}
				else if (indexAccesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					indices.push_back(static_cast<uint32_t>((reinterpret_cast<uint32_t const*>(indexes + (i * indexByteStride)))[0]));
				}
			}

			VertexBufferCreationDesc vertexBufferCreationDesc
			{
				.numberOfElements = static_cast<uint32_t>(vertices.size()),
				.stride = sizeof(Vertex)
			};

			IndexBufferCreationDesc indexBufferCreationDesc
			{
				.bufferSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t)),
				.indicesCount = static_cast<uint32_t>(indices.size()),
				.format = DXGI_FORMAT_R32_UINT,
			};

			std::unique_ptr<VertexBuffer> modelVertexBuffer = engine->CreateVertexBuffer(vertexBufferCreationDesc, vertices.data(), L"Vertex Buffer");
			std::unique_ptr<IndexBuffer> modelIndexBuffer = engine->CreateIndexBuffer(indexBufferCreationDesc, indices.data(), L"Index Buffer");

			mVertexBuffers.push_back(std::move(modelVertexBuffer));
			mIndexBuffers.push_back(std::move(modelIndexBuffer));
		}
	}
}