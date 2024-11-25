#pragma once
#include "DX12Include.h"
#include "DirectionalLight.h"
#include "Transform.h"
#include "TransformationMatrix.h"
#include "TextureManager.h"
#include "Material.h"
#include "VertexData.h"
#include "ModelData.h"
#include "Camera.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// 円周率
#define pi 3.141592653589793238462643383279502884197169399375105820974944f


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Model;
class Object3DCommon;

/// -------------------------------------------------------------
///						オブジェクト3Dクラス
/// -------------------------------------------------------------
class Object3D
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3dCommon, const std::string& fileName);

	// 更新処理
	void Update();

	// ImGui
	void DrawImGui();

	// カメラ専用のImGui
	void CameraImGui();

	// ドローコール
	void DrawCall(ID3D12GraphicsCommandList* commandList);

public: /// ---------- 設定処理 ---------- ///

	// 共通描画設定
	void SetObject3DBufferData(ID3D12GraphicsCommandList* commandList);

	// モデルの追加
	void SetModel(const std::string& filePath);

	// 位置を設定
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }

	// カメラの設定
	void SetCamera(Camera* camera) { camera_ = camera; }

public: /// ---------- ゲッタ ---------- ///

private: /// ---------- メンバ変数 ---------- ///

	// 下4つの関数をまとめる関数
	void preInitialize(DirectXCommon* dxCommon);

	// 頂点データの初期化処理
	void InitializeMaterial(DirectXCommon* dxCommon);

	// 頂点バッファデータの初期化
	void InitializeVertexBufferData(DirectXCommon* dxCommon);

	// マテリアルの初期化処理
	void InitializeTransfomation(DirectXCommon* dxCommon);

	// 平行光源の初期化処理
	void ParallelLightSorce(DirectXCommon* dxCommon);

private: /// ---------- メンバ変数 ---------- ///

	// 分割数
	uint32_t kSubdivision = 32;

	// 緯度・経度の分割数に応じた角度の計算
	float kLatEvery = pi / float(kSubdivision);
	float kLonEvery = 2.0f * pi / float(kSubdivision);

	// 球体の頂点数の計算
	uint32_t TotalVertexCount = kSubdivision * kSubdivision * 6;

private: /// ---------- メンバ変数 ---------- ///

	Object3DCommon* object3dCommon_ = nullptr;

	std::shared_ptr<Model> model_;

	Camera* camera_ = nullptr;

	Transform transform;
	Transform cameraTransform;

	// バッファリソースの作成
	ComPtr <ID3D12Resource> vertexResource;
	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> directionalLightResource;

	// wvpデータを書き込む
	TransformationMatrix* wvpData = nullptr;

	// OBJファイルのデータ
	ModelData modelData;

	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	// ライトデータ
	DirectionalLight* directionalLightData = nullptr;

};

