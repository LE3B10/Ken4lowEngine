#include "AssimpLoader.h"

#include <cassert>
#include <cctype>
#include <stdexcept>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	/// 			Assimpを使ったモデル読み込みクラス
	/// -------------------------------------------------------------
	ModelData AssimpLoader::LoadModel(const std::string& modelFilePath)
	{
		// 拡張子を取得（小文字化して判定）
		std::string extension;
		{
			auto dot = modelFilePath.find_last_of('.');
			if (dot != std::string::npos)
			{
				extension = modelFilePath.substr(dot + 1);
				for (char& c : extension) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			}
		}

		Assimp::Importer importer;
		const std::string filePath = "Resources/Models/" + modelFilePath;

		// 共通ポストプロセス
		// NOTE:
		// - UVが無いモデルもあり得るので、読み込み後のパース側で必ずガードする
		// - 必要なら aiProcess_GenUVCoords を追加して「自動UV生成」も可能（品質は用途次第）
		const unsigned int kFlags =
			aiProcess_Triangulate |			  // 三角形化
			aiProcess_JoinIdenticalVertices | // 重複頂点の除去
			aiProcess_GenSmoothNormals |      // 法線を自動生成
			aiProcess_FlipWindingOrder |	  // 頂点の順番を反転
			aiProcess_FlipUVs;				  // UVの上下反転
		// | aiProcess_GenUVCoords;        // ←UVが無いモデルを強制対応したいなら

		const aiScene* scene = nullptr;

		if (extension == "obj" || extension == "gltf" || extension == "glb")
		{
			scene = importer.ReadFile(filePath.c_str(), kFlags);
		}
		else
		{
			throw std::runtime_error("Unsupported file format: " + extension);
		}

		// 読み込み失敗 or 不完全なシーン
		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
		{
			throw std::runtime_error(std::string("Assimp ReadFile failed: ") + importer.GetErrorString());
		}
		if (!scene->HasMeshes())
		{
			throw std::runtime_error("Assimp scene has no meshes: " + filePath);
		}

		ModelData modelData;

		// メッシュ解析
		ParseMeshes(scene, modelData);

		// ノード階層を構築
		modelData.rootNode = ReadNode(scene->mRootNode);

		return modelData;
	}

	/// -------------------------------------------------------------
	///						ノードを読み込む
	/// -------------------------------------------------------------
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

	/// -------------------------------------------------------------
	///					メッシュを解析する
	/// -------------------------------------------------------------
	void AssimpLoader::ParseMeshes(const aiScene* scene, ModelData& modelData)
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

			// 頂点データ
			for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
			{
				// 位置（Assimpは必ず mVertices を持つ想定だが念のため）
				const aiVector3D& position = mesh->mVertices[vertexIndex];
				sub.vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };

				// 法線（無い場合のフォールバック）
				if (hasNormals)
				{
					const aiVector3D& normal = mesh->mNormals[vertexIndex];
					sub.vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
				}
				else
				{
					sub.vertices[vertexIndex].normal = { 0.0f, 1.0f, 0.0f };
				}

				// UV（nullptr対策：無いメッシュもある）
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

			// インデックス
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
			{
				const aiFace& face = mesh->mFaces[faceIndex];
				assert(face.mNumIndices == 3);

				for (uint32_t element = 0; element < face.mNumIndices; ++element)
				{
					const uint32_t vi = face.mIndices[element];
					// 念のため範囲チェック（壊れたデータ防止）
					if (vi < mesh->mNumVertices)
					{
						sub.indices.push_back(vi);
					}
				}
			}

			// SkinCluster（ボーンがある場合のみ）
			if (mesh->HasBones() && mesh->mBones != nullptr)
			{
				for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
				{
					aiBone* bone = mesh->mBones[boneIndex];
					if (!bone) { continue; }

					const std::string jointName = bone->mName.C_Str();
					JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

					// InverseBindPoseMatrix の抽出
					const aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();

					aiVector3D scale, translate;
					aiQuaternion rotate;
					bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

					Matrix4x4 bindPoseMatrix = Matrix4x4::MakeAffineMatrix(
						{ scale.x, scale.y, scale.z },
						{ rotate.x, -rotate.y, -rotate.z, rotate.w },
						{ -translate.x, translate.y, translate.z }
					);

					jointWeightData.inverseBindPoseMatrix = Matrix4x4::Inverse(bindPoseMatrix);

					// Weight
					if (bone->mWeights != nullptr)
					{
						for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
						{
							const auto& w = bone->mWeights[weightIndex];
							if (w.mVertexId < mesh->mNumVertices)
							{
								jointWeightData.vertexWeights.push_back(
									{ w.mWeight, baseVertex + w.mVertexId }
								);
							}
						}
					}
				}
			}

			// マテリアル（Diffuseテクスチャ）
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			if (material && material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString aiTexPath;
				if (material->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS)
				{
					std::filesystem::path texPath(aiTexPath.C_Str());
					sub.material.textureFilePath = texPath.filename().string();
				}
				else
				{
					sub.material.textureFilePath = "";
				}
			}
			else
			{
				sub.material.textureFilePath = "";
			}

			modelData.subMeshes.push_back(std::move(sub));
			baseVertex += mesh->mNumVertices;
		}
	}

} // namespace Ken4lowEngine
