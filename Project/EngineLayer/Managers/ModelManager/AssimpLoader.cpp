#include "AssimpLoader.h"
#include "TextureManager.h"

#include <cassert>
#include <cctype>
#include <stdexcept>

namespace Ken4lowEngine
{

	ModelData AssimpLoader::LoadModel(const std::string& modelFilePath)
	{
		std::string extension;
		{
			auto dot = modelFilePath.find_last_of('.');
			if (dot != std::string::npos)
			{
				extension = modelFilePath.substr(dot + 1);
				for (char& c : extension)
				{
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
			}
		}

		Assimp::Importer importer;
		const std::string filePath = "Resources/Models/" + modelFilePath;

		const unsigned int kFlags =
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_GenSmoothNormals |
			aiProcess_FlipWindingOrder |
			aiProcess_FlipUVs;

		const aiScene* scene = nullptr;

		if (extension == "obj" || extension == "gltf" || extension == "glb")
		{
			scene = importer.ReadFile(filePath.c_str(), kFlags);
		}
		else
		{
			throw std::runtime_error("Unsupported file format: " + extension);
		}

		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
		{
			throw std::runtime_error(std::string("Assimp ReadFile failed: ") + importer.GetErrorString());
		}
		if (!scene->HasMeshes())
		{
			throw std::runtime_error("Assimp scene has no meshes: " + filePath);
		}

		ModelData modelData;
		ParseMeshes(scene, modelData, modelFilePath);
		modelData.rootNode = ReadNode(scene->mRootNode);

		return modelData;
	}

	Node AssimpLoader::ReadNode(aiNode* node)
	{
		Node result;

		aiVector3D scale, translate;
		aiQuaternion rotate;
		node->mTransformation.Decompose(scale, rotate, translate);

		result.transform.scale = { scale.x, scale.y, scale.z };
		result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };
		result.transform.translate = { -translate.x, translate.y, translate.z };

		result.localMatrix = Matrix4x4::MakeAffineMatrix(
			result.transform.scale,
			result.transform.rotate,
			result.transform.translate);

		result.name = node->mName.C_Str();
		result.children.resize(node->mNumChildren);

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			result.children[i] = ReadNode(node->mChildren[i]);
		}

		return result;
	}

	void AssimpLoader::ParseMeshes(const aiScene* scene, ModelData& modelData, const std::string& modelFilePath)
	{
		uint32_t baseVertex = 0;

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh) { continue; }

			SubMesh sub;
			sub.vertices.resize(mesh->mNumVertices);

			const bool hasNormals = (mesh->HasNormals() && mesh->mNormals != nullptr);
			const bool hasUV0 = (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr);

			for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
			{
				const aiVector3D& position = mesh->mVertices[vertexIndex];
				sub.vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };

				if (hasNormals)
				{
					const aiVector3D& normal = mesh->mNormals[vertexIndex];
					sub.vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
				}
				else
				{
					sub.vertices[vertexIndex].normal = { 0.0f, 1.0f, 0.0f };
				}

				if (hasUV0)
				{
					const aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
					sub.vertices[vertexIndex].texcoord = { texcoord.x, texcoord.y };
				}
				else
				{
					sub.vertices[vertexIndex].texcoord = { 0.0f, 0.0f };
				}
			}

			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
			{
				const aiFace& face = mesh->mFaces[faceIndex];
				assert(face.mNumIndices == 3);

				for (uint32_t element = 0; element < face.mNumIndices; ++element)
				{
					const uint32_t vi = face.mIndices[element];
					if (vi < mesh->mNumVertices)
					{
						sub.indices.push_back(vi);
					}
				}
			}

			if (mesh->HasBones() && mesh->mBones != nullptr)
			{
				for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
				{
					aiBone* bone = mesh->mBones[boneIndex];
					if (!bone) { continue; }

					const std::string jointName = bone->mName.C_Str();
					JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

					const aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();

					aiVector3D scale, translate;
					aiQuaternion rotate;
					bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

					Matrix4x4 bindPoseMatrix = Matrix4x4::MakeAffineMatrix(
						{ scale.x, scale.y, scale.z },
						{ rotate.x, -rotate.y, -rotate.z, rotate.w },
						{ -translate.x, translate.y, translate.z });

					jointWeightData.inverseBindPoseMatrix = Matrix4x4::Inverse(bindPoseMatrix);

					if (bone->mWeights != nullptr)
					{
						for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
						{
							const auto& w = bone->mWeights[weightIndex];
							if (w.mVertexId < mesh->mNumVertices)
							{
								jointWeightData.vertexWeights.push_back(
									{ w.mWeight, baseVertex + w.mVertexId });
							}
						}
					}
				}
			}

			aiMaterial* material = nullptr;
			if (mesh->mMaterialIndex < scene->mNumMaterials)
			{
				material = scene->mMaterials[mesh->mMaterialIndex];
			}

			if (material && material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString aiTexPath;
				if (material->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS)
				{
					sub.material.textureFilePath = ResolveModelTexturePath(modelFilePath, aiTexPath);
				}
				else
				{
					sub.material.textureFilePath.clear();
				}
			}
			else
			{
				sub.material.textureFilePath.clear();
			}

			modelData.subMeshes.push_back(std::move(sub));
			baseVertex += mesh->mNumVertices;
		}
	}

	std::string AssimpLoader::ResolveModelTexturePath(
		const std::string& modelFilePath,
		const aiString& aiTexPath)
	{
		std::filesystem::path texPath(aiTexPath.C_Str());

		const std::string raw = texPath.generic_string();
		if (!raw.empty() && raw[0] == '*')
		{
			return "";
		}

		texPath.replace_extension(".dds");

		std::filesystem::path modelDir = std::filesystem::path(modelFilePath).parent_path();
		std::filesystem::path relativePath;

		if (texPath.is_absolute())
		{
			relativePath = texPath.filename();
		}
		else
		{
			relativePath = (modelDir / texPath).lexically_normal();
		}

		TextureManager* texMgr = TextureManager::GetInstance();

		// 1. モデル相対パスで探す
		std::string found = texMgr->FindCompiledTexturePath(relativePath.generic_string());
		if (!found.empty())
		{
			return found;
		}

		// 2. ファイル名だけで探す
		found = texMgr->FindCompiledTexturePath(texPath.filename().generic_string());
		if (!found.empty())
		{
			return found;
		}

		// 3. 最後の保険
		return "Resources/Textures/Compiled/Debug/uvChecker.dds";
	}

} // namespace Ken4lowEngine