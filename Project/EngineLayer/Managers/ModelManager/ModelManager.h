#pragma once
#include "VertexData.h"
#include "ModelData.h"

#include "Model.h"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
	std::shared_ptr<Model> FindModel(const std::string& filePath);

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

public: /// ---------- メンバ関数（Assimp）---------- ///

	// モデルファイル読み込み
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	// Objファイルを読み込む関数
	static ModelData LoadObjFile1(const std::string& directoryPath, const std::string& filename);

	// .gltfファイルを読み込む関数
	static ModelData LoadGLTFFile(const std::string& directoryPath, const std::string& filename);

private: /// ---------- メンバ関数（Assimp用）---------- ///

	// AssimpのNodeから構造体に変換する関数
	static Node ReadNode(aiNode* node);

	// 
	static void ParseMeshes(const aiScene* scene, const std::string& directoryPath, ModelData& modelData);

	// gltf特有の処理
	static void HandleGltfSpecifics(const aiScene* scene);

	// obj特有の処理
	static void HandleObjSpecifics(const aiScene* scene);

	
private: /// ---------- メンバ変数 ---------- ///

	std::unordered_map<std::string, std::shared_ptr<Model>> models_;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(const ModelManager&) = delete;
	const ModelManager& operator=(const ModelManager&) = delete;
};

