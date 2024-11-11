#pragma once
#include "DX12Include.h"
#include "DirectionalLight.h"
#include "Transform.h"
#include "TransformationMatrix.h"
#include "TextureManager.h"
#include "Material.h"
#include "VertexData.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f

/// -------------------------------------------------------------
///						オブジェクト3Dクラス
/// -------------------------------------------------------------
class Object3D
{
public: /// ---------- 構造体 ---------- ///

	// MaterialDataの構造体
	struct MaterialData
	{
		std::string textureFilePath;
	};

	// ModelData構造体
	struct ModelData
	{
		std::vector<VertexData> vertices;
		MaterialData material;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initilize();

	// 更新処理
	void Update();

	// ImGui
	void DrawImGui();

	// 共通描画設定
	void SetObject3DBufferData(ID3D12GraphicsCommandList* commandList);

	// .objファイルの読み取り
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private: /// ---------- メンバ関数 ---------- ///

	// .mtlファイルの読み取り
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);


private: /// ---------- メンバ変数 ---------- ///

	Transform transform;
	Transform cameraTransform;

	//データを書き込む
	TransformationMatrix* wvpData = nullptr;

	// OBJファイルのデータ
	ModelData modelData;
	// バッファリソースの作成
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource;
	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	DirectionalLight* directionalLightData = nullptr;

	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource;
	Microsoft::WRL::ComPtr <ID3D12Resource> directionalLightResource;

};

