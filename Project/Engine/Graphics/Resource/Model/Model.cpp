#define NOMINMAX
#include "Model.h"
#include "AssimpLoader.h"
#include "TextureManager.h"

#include <algorithm>
#include <limits>

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

		BuildLocalBounds();

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

	void Model::BuildLocalBounds()
	{
		Vector3 minPos{
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max()
		};
		Vector3 maxPos{
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest()
		};
		bool hasVertex = false;

		for (const auto& sub : modelData_.subMeshes)
		{
			for (const auto& vertex : sub.vertices)
			{
				const Vector3 position{ vertex.position.x, vertex.position.y, vertex.position.z };
				minPos.x = std::min(minPos.x, position.x);
				minPos.y = std::min(minPos.y, position.y);
				minPos.z = std::min(minPos.z, position.z);
				maxPos.x = std::max(maxPos.x, position.x);
				maxPos.y = std::max(maxPos.y, position.y);
				maxPos.z = std::max(maxPos.z, position.z);
				hasVertex = true;
			}
		}

		if (!hasVertex)
		{
			localBounds_ = { {}, 1.0f };
			hasLocalBounds_ = false;
			return;
		}

		hasLocalBounds_ = true;
		localBounds_.center = (minPos + maxPos) * 0.5f;
		localBounds_.radius = Vector3::Length(maxPos - localBounds_.center) * 1.1f;
		if (localBounds_.radius <= 0.001f)
		{
			localBounds_.radius = 1.0f;
		}
	}

}
