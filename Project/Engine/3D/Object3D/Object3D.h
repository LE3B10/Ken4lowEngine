#pragma once
#include "DX12Include.h"
#include "DirectionalLight.h"
#include "Transform.h"
#include "TransformationMatrix.h"
#include "TextureManager.h"
#include "Material.h"
#include "VertexData.h"
#include "ModelData.h"
#include "MainCamera3D.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Model;

/// -------------------------------------------------------------
///						オブジェクト3Dクラス
/// -------------------------------------------------------------
class Object3D
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& fileName);

	// 更新処理
	void Update();

	// ImGui
	void DrawImGui();

	void CameraImGui();

	// ドローコール
	void DrawCall(ID3D12GraphicsCommandList* commandList);

	// 共通描画設定
	void SetObject3DBufferData(ID3D12GraphicsCommandList* commandList);

	// モデルの追加
	void SetModel(const std::string& filePath);

	// 位置を設定
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }

	void SetScale(const Vector3& scale) { transform.scale = scale; }

private: /// ---------- メンバ変数 ---------- ///

	std::shared_ptr<Model> model_;

	MainCamera3D* camera3D_ = nullptr;

	Transform transform;
	Transform cameraTransform;

	//データを書き込む
	TransformationMatrix* wvpData = nullptr;

	// OBJファイルのデータ
	ModelData modelData;
	// バッファリソースの作成
	ComPtr <ID3D12Resource> vertexResource;
	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	DirectionalLight* directionalLightData = nullptr;

	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> directionalLightResource;

};

