#include "AssimpLoader.h"

ModelData AssimpLoader::LoadModel(const std::string& modelFilePath)
{
	// 1. ファイルの拡張子を取得して判定
	std::string extension = modelFilePath.substr(modelFilePath.find_last_of('.') + 1);

	// 2. Assimp でモデルを読み込む
	Assimp::Importer importer;
	std::string filePath = "Resources/Models/" + modelFilePath;
	const aiScene* scene = nullptr;

	if (extension == "obj")
	{
		// OBJ ファイル読み込み
		scene = importer.ReadFile(filePath.c_str(),
			aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	}
	else if (extension == "gltf" || extension == "glb")
	{
		// glTF ファイル読み込み (追加フラグあり)
		scene = importer.ReadFile(filePath.c_str(),
			aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	}
	else
	{
		throw std::runtime_error("Unsupported file format: " + extension);
	}

	// ファイル読み込み結果をチェック
	assert(scene && scene->HasMeshes());

	// 3. モデルデータ構造体を作成
	ModelData modelData;

	// 4. メッシュを解析 (共通処理)
	ParseMeshes(scene, modelData);

	// 5. ノード階層を構築 (共通処理)
	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

Node AssimpLoader::ReadNode(aiNode* node)
{
	Node result;

	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate); // assimpの行列からSRTを抽出する関数を利用
	result.transform.scale = { scale.x, scale.y, scale.z }; // Scaleはそのまま
	result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w }; // x軸を反転、さらに回転方向が逆なので軸を反転させる
	result.transform.translate = { -translate.x, translate.y, translate.z }; // x軸を反転
	result.localMatrix = Matrix4x4::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);
	result.name = node->mName.C_Str();
	result.children.resize(node->mNumChildren);

	// 子ノードを再帰的に処理
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

void AssimpLoader::ParseMeshes(const aiScene* scene, ModelData& modelData)
{
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals() && mesh->HasTextureCoords(0));
		uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());

		// 頂点データの解析
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
		{
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

			VertexData vertexData;
			vertexData.position = { -position.x, position.y, position.z, 1.0f };
			vertexData.normal = { -normal.x, normal.y, normal.z };
			vertexData.texcoord = { texcoord.x, texcoord.y };

			modelData.vertices.push_back(vertexData);
		}

		// インデックスデータの解析
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形のみ対応

			for (uint32_t element = 0; element < face.mNumIndices; ++element)
			{
				uint32_t vertexIndex = face.mIndices[element];
				modelData.indices.push_back(vertexOffset + vertexIndex);
			}
		}

		// SkinCluster構築用のデータを取得
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			// Jointごとの格納領域を作る
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			// InverseBindPoseMatrixの抽出
			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
			Matrix4x4 bindPoseMatrix = Matrix4x4::MakeAffineMatrix({ scale.x, scale.y, scale.z }, { rotate.x, -rotate.y, -rotate.z, rotate.w }, { -translate.x, translate.y, translate.z });
			jointWeightData.inverseBindPoseMatrix = Matrix4x4::Inverse(bindPoseMatrix);

			// Weight情報を取り出す
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
			{
				jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId });
			}
		}

		// マテリアルの解析
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString aiTexPath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath);

			// フォルダー情報は捨ててファイル名だけ保持
			std::filesystem::path texPath(aiTexPath.C_Str());
			modelData.material.textureFilePath = texPath.filename().string();
		}
		else
		{
			modelData.material.textureFilePath = ""; // デフォルト値
		}
	}
}
