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

// note(rtarun9) : Not sure if having a vector of wstring's is a good idea for material's.
// Doing this to keep all loader's be separated from each other.
struct Material
{
	std::wstring albedoTexturePaths{};
};

struct Mesh
{
	std::vector<Vertex> vertices{};
	std::vector<uint32_t> indices{};
	uint32_t materialIndex{};
};

namespace nether::loaders
{
	// Model Loading for GLTF model's, using tinygltf.
	class ModelLoader
	{
	public:
		ModelLoader(const std::wstring_view modelAssetPath);

		void LoadNode(const uint32_t nodeIndex, tinygltf::Model* const model);
		void LoadMaterials(tinygltf::Model* const model);

	public:
		std::vector<Mesh> mMeshes{};
		std::vector<Material> mMaterials{};

		std::wstring mModelDirectory{};
	};
}