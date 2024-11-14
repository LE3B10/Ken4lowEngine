#pragma once
#include "VertexData.h"
#include "ModelData.h"

#include <fstream>
#include <sstream>
#include <string>

/// -------------------------------------------------------------
///					モデルマネージャークラス
/// -------------------------------------------------------------
class ModelManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static ModelManager* GetInstance();

	// 初期化処理
	void Initialize();

	// .objファイルの読み取り
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

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
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

private:


};

