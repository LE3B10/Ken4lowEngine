#pragma once
#include "ModelData.h"

#include <string>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class AssimpLoader
{
public: /// ---------- メンバ関数 ---------- ///

	// Assimpでモデルを読み込む
	static ModelData LoadModel(const std::string& directoryPath, const std::string& filename);

private: /// ---------- メンバ関数 ---------- ///

	// ノードを読み込む
	static Node ReadNode(aiNode* node);

	// メッシュを解析する
	static void ParseMeshes(const aiScene* scene, const std::string& directoryPath, ModelData& modelData);

	// glTF特有の処理
	static void HandleGltfSpecifics(const aiScene* scene);

	// OBJ特有の処理
	static void HandleObjSpecifics(const aiScene* scene);
};

