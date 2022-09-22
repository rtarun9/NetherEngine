#include "Pch.hpp"

#include "GameObject.hpp"

#include "Loaders/ModelLoader.hpp"
#include "Rendering/Device.hpp"
#include "Rendering/Resources.hpp"
#include "Common/Utils.hpp"

namespace nether::scene
{
	GameObject::GameObject(rendering::Device* const device, const std::wstring_view modelPath, const std::wstring_view objectName)
	{
		// Create default texture (to be used if any texture is missing / not found).
		// Also create the cube game object.
		if (!sDefaultTexture)
		{
			// Get assets directory (same as done in Engine, but have to do it again as I dont want to pass the whole engine into this path.
			// note(rtarun9) : This is done like this for now just to get things working, and is probably going to change at some time. As this is a one time operation, the overhead is pretty low,
			// and will in no way affect performance in the current engine state.
			std::filesystem::path currentDirectory = std::filesystem::current_path();

			while (!std::filesystem::exists(currentDirectory / "Assets"))
			{
				if (currentDirectory.has_parent_path())
				{
					currentDirectory = currentDirectory.parent_path();
				}
				else
				{
					utils::ErrorMessage(L"Assets Directory not found!");
				}
			}

			const std::filesystem::path assetsDirectory = currentDirectory / "Assets";

			if (!std::filesystem::is_directory(assetsDirectory))
			{
				utils::ErrorMessage(L"Assets Directory that was located is not a directory!");
			}

			const std::wstring defaultTexturePath = assetsDirectory.wstring() + L"/Textures/Prototype.png";

			// Load the default texture.
			sDefaultTexture = std::make_unique<rendering::Texture>(device->CreateTexture(defaultTexturePath));
		}

		const std::string modelPathStr = utils::WStringToString(modelPath);
		if (modelPathStr.find_last_of("/\\") != std::string::npos)
		{
			const std::string modelDirectoryPathStr = modelPathStr.substr(0, modelPathStr.find_last_of("/\\")) + "/";
			mModelDirectory = utils::StringToWString(modelDirectoryPathStr);
		}

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
		const size_t materialCount = modelLoader.mMaterials.size();

		mMeshCount = meshesCount;

		mVertexBuffers.reserve(meshesCount);
		mIndexBuffers.reserve(meshesCount);
		mMaterialIndices.reserve(meshesCount);

		mAlbedoTextures.reserve(materialCount);

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
			mMaterialIndices.push_back(modelLoader.mMeshes[i].materialIndex);
		}

		for (size_t i : std::views::iota(0u, materialCount))
		{
			if (modelLoader.mMaterials[i].albedoTexturePaths != L"")
			{
				rendering::Texture albedoTexture = device->CreateTexture(modelLoader.mMaterials[i].albedoTexturePaths);
				mAlbedoTextures.push_back(std::move(albedoTexture));
			}
			else
			{
				mAlbedoTextures.push_back(*sDefaultTexture.get());
			}
		}
	}
}