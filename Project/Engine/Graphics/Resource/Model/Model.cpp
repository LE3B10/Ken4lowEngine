#include "Model.h"
#include "AssimpLoader.h"
#include "TextureManager.h"

namespace Ken4lowEngine
{

	void Model::Initialize(const std::string& filePath)
	{
		modelData_ = AssimpLoader::LoadModel(filePath);

		meshes_.clear();
		materialSRVs_.clear();

		meshes_.reserve(modelData_.subMeshes.size());
		materialSRVs_.reserve(modelData_.subMeshes.size());

		static const std::string kDefaultTexturePath = "Effects/white.dds";

		for (const auto& sub : modelData_.subMeshes)
		{
			std::string texturePath = sub.material.textureFilePath;
			if (texturePath.empty())
			{
				texturePath = kDefaultTexturePath;
			}

			TextureManager::GetInstance()->LoadTexture(texturePath);
			materialSRVs_.push_back(TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));

			Mesh mesh{};
			mesh.Initialize(sub.vertices, sub.indices);
			meshes_.push_back(std::move(mesh));
		}
	}

}