#pragma once
#include "ModelData.h"

#include <string>
#include <filesystem>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

/// -------------------------------------------------------------
/// 				Assimpでモデルを読み込むクラス
/// -------------------------------------------------------------
class AssimpLoader
{
public: /// ---------- メンバ関数 ---------- ///

	// Assimpでモデルを読み込む
	static ModelData LoadModel(const std::string& modelFilePath);

private: /// ---------- メンバ関数 ---------- ///

	// ノードを読み込む
	static Node ReadNode(aiNode* node);

	// メッシュを解析する
	static void ParseMeshes(const aiScene* scene, ModelData& modelData);
};

