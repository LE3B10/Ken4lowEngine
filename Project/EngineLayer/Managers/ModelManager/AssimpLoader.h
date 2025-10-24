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

	/// <summary>
	/// Assimpを使ってモデルデータを読み込む
	/// </summary>
	/// <param name="modelFilePath">モデルファイルパス</param>
	/// <returns></returns>
	static ModelData LoadModel(const std::string& modelFilePath);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ノードを読み込む
	/// </summary>
	/// <param name="node">ノード</param>
	/// <returns></returns>
	static Node ReadNode(aiNode* node);

	/// <summary>
	/// メッシュを解析する
	/// </summary>
	/// <param name="scene">シーン</param>
	/// <param name="modelData">モデルデータ</param>
	static void ParseMeshes(const aiScene* scene, ModelData& modelData);
};

