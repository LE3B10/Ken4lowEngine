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
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Model;
class Object3DCommon;

/// -------------------------------------------------------------
///						オブジェクト3Dクラス
/// -------------------------------------------------------------
class Object3D
{
public: /// ---------- 構造体 ---------- ///

	// シェーダー側のカメラ構造体
	struct CameraForGPU
	{
		Vector3 worldPosition;
	};

	// シェーダー側の点光源の構造体
	struct PointLight
	{
		Vector4 color; // ライトの色
		Vector3 position; // ライトの位置
		float intensity; // 輝度
		float radius; // 有効範囲
		float decay; // 減衰率
		float padding[2]; // パディング
	};

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
	void Draw();

public: /// ---------- 設定処理 ---------- ///

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

	// カメラ用のリソース生成
	void InitializeCameraResource(DirectXCommon* dxCommon);

	// ポイントライトの初期化処理
	void PointLightSource(DirectXCommon* dxCommon);

private: /// ---------- メンバ変数 ---------- ///

	// 分割数
	uint32_t kSubdivision = 32;

	// 緯度・経度の分割数に応じた角度の計算
	float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);
	float kLonEvery = 2.0f * std::numbers::pi_v<float> / float(kSubdivision);

	// 球体の頂点数の計算
	uint32_t TotalVertexCount = kSubdivision * kSubdivision * 6;

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon = nullptr;
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
	ComPtr <ID3D12Resource> cameraResource;
	ComPtr <ID3D12Resource> pointLightResource;

	// wvpデータを書き込む
	// カメラにデータを書き込む
	CameraForGPU* cameraData = nullptr;
	TransformationMatrix* wvpData = nullptr;
	PointLight* pointLightData = nullptr;

	// OBJファイルのデータ
	ModelData modelData;

	// 頂点リソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	// ライトデータ
	DirectionalLight* directionalLightData = nullptr;

};

