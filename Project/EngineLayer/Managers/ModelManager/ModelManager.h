#pragma once
#include "VertexData.h"
#include "ModelData.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <algorithm>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Object3D;

/// -------------------------------------------------------------
///					モデルマネージャークラス
/// -------------------------------------------------------------
class ModelManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static ModelManager* GetInstance();

	// .objファイルの読み込み
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// モデルファイルの読み込み
	void LoadModel(const std::string& filePath);

	// モデルの検索
	std::shared_ptr<Object3D> FindModel(const std::string& filePath);

private: /// ---------- 静的メンバ関数 ---------- ///

	// 頂点データを解析する関数
	static Vector4 ParseVertex(std::istringstream& s);

	// テクスチャ座標データを解析する関数
	static Vector2 ParseTexcoord(std::istringstream& s);

	// 法線データを解析する関数
	static Vector3 ParseNormal(std::istringstream& s);

	// 面情報を解析し、頂点データリストに展開して追加する関数
	static void ParseFace(std::istringstream& s, const std::vector<Vector4>& positions, const std::vector<Vector2>& texcoords, const std::vector<Vector3>& normals, std::vector<VertexData>& vertices);

	// .mtlファイルの読み取り
	static Material LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

private: /// ---------- メンバ変数 ---------- ///

	std::unordered_map<std::string, std::shared_ptr<Object3D>> models_;

	const std::string directoryPath = "Resources";

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(const ModelManager&) = delete;
	const ModelManager& operator=(const ModelManager&) = delete;
};

