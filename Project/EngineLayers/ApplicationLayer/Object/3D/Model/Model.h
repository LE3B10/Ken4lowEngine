#pragma once
#include "DX12Include.h"
#include "VertexData.h"
#include "ModelData.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Transform.h"
#include "TransformationMatrix.h"
#include "DirectionalLight.h"
#include "Material.h"
#include <numbers>

class DirectXCommon;

//#define pi 3.141592653589793238462643383279502884197169399375105820974944f

/// -------------------------------------------------------------
///						　3Dモデルクラス
/// -------------------------------------------------------------
class Model
{
public: /// ---------- 構造体 ---------- ///

	
	
public: /// ---------- メンバ関数 ---------- ///

	//　初期化処理
	void Initialize(const std::string& directoryPath, const std::string& filename);

	// 更新処理
	void Update();

	// 共通描画設定
	void SetBufferData(ID3D12GraphicsCommandList* commandList);

	// 描画処理
	void DrawCall(ID3D12GraphicsCommandList* commandList);

	// カメラ操作
	void CameraImGui();

	// ImGUiの描画
	void DrawImGui();

	

public: /// ---------- ゲッタ ---------- ///

	const Vector3& GetScale() const { return transform.scale; }
	const Vector3& GetRotate() const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }

public: /// ---------- セッタ ---------- ///

	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }

private:

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
	float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);
	float kLonEvery = 2.0f * std::numbers::pi_v<float> / float(kSubdivision);

	// 球体の頂点数の計算
	uint32_t TotalVertexCount = kSubdivision * kSubdivision * 6;

private: /// ---------- メンバ変数 ---------- ///

	// バッファリソースの作成
	ComPtr <ID3D12Resource> vertexResource;
	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> directionalLightResource;

	Transform transform;
	//Transform cameraTransform;

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

