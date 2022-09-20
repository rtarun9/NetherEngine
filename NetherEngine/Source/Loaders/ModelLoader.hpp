#pragma once

// Forward declaration's.
namespace tinygltf
{
	class Model;
	class Node;
}

struct Vertex
{
	DirectX::XMFLOAT3 position{};
	DirectX::XMFLOAT2 textureCoord{};
	DirectX::XMFLOAT3 normal{};
};

struct Mesh
{
	std::vector<Vertex> vertices{};
	std::vector<uint32_t> indices{};
};

namespace nether::loaders
{
	// Model Loading for GLTF model's, using tinygltf.
	class ModelLoader
	{
	public:
		ModelLoader(const std::wstring_view modelAssetPath);

		void LoadNode(const uint32_t nodeIndex, tinygltf::Model* const model);

	public:
		std::vector<Mesh> mMeshes{};
	};
}