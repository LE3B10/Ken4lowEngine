#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "TextureManager.h"
#include "Material.h"
#include "Mesh.h"
#include "VertexData.h"
#include "ModelData.h"
#include "Camera.h"
#include "LightManager.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Model;
class Object3DCommon;
class SkyBox;


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

	// ディゾルブの設定
	struct DissolveSetting
	{
		float threshold;        // 閾値
		float edgeThickness;    // エッジの太さ
		float padding0[2];      // パディング
		Vector4 edgeColor;      // 色
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& fileName);

	// 更新処理
	void Update();

	// ImGui
	void DrawImGui();

	// 描画処理
	void Draw();

public: /// ---------- 設定処理 ---------- ///

	// モデルの追加
	void SetModel(const std::string& filePath);

	// スケールを設定
	void SetScale(const Vector3& scale) { worldTransform.scale_ = scale; }
	Vector3 GetScale() const { return worldTransform.scale_; }

	// 回転を設定
	void SetRotate(const Vector3& rotate) { worldTransform.rotate_ = rotate; }
	Vector3 GetRotate() const { return worldTransform.rotate_; }

	// 位置を設定
	void SetTranslate(const Vector3& translate) { worldTransform.translate_ = translate; }
	Vector3 GetTranslate() const { return worldTransform.translate_; }

	// 色を設定
	void SetColor(const Vector4& color) { material_.SetColor(color); }

	// カメラの設定
	void SetCamera(Camera* camera) { camera_ = camera; }

	// 反射率の設定
	void SetReflectivity(float reflectivity) { material_.SetReflection(reflectivity); }

	// 全サブメッシュを同じテクスチャに差し替える
	void SetTextureForAll(const std::string& texturePath);

	// 指定サブメッシュだけ差し替える（必要なら）
	void SetTextureForSubmesh(size_t index, const std::string& texturePath);

	// サブメッシュ数の取得（UI で使うなら）
	size_t GetSubmeshCount() const { return meshes_.size(); }

public: /// ---------- ディゾルブの設定 ---------- ///

	// ディゾルブの閾値を設定
	void SetDissolveThreshold(float threshold) { dissolveSetting_->threshold = threshold; }
	// エッジの太さを設定
	void SetDissolveEdgeThickness(float thickness) { dissolveSetting_->edgeThickness = thickness; }
	// エッジの色を設定
	void SetDissolveEdgeColor(const Vector4& color) { dissolveSetting_->edgeColor = color; }

private: /// ---------- メンバ変数 ---------- ///

	// カメラ用のリソース生成
	void InitializeCameraResource();

	// ディゾルブ用のリソース生成
	void InitializeDissolveResource();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;
	SkyBox* skyBox_ = nullptr;

	std::shared_ptr<Object3D> model_;

	// マテリアルデータ
	Material material_;

	// ワールドトランスフォーム
	WorldTransform worldTransform;

	// メッシュ
	std::vector<Mesh> meshes_;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;

	// バッファリソースの作成
	ComPtr <ID3D12Resource> cameraResource;

	// カメラにデータを書き込む
	CameraForGPU* cameraData = nullptr;

	// モデルデータ（subMeshes を想定）
	ModelData modelData;

	float alpha = 1.0f; // α値

	// 環境マップのテクスチャ
	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};

	// ディゾルブマスクのテクスチャ
	D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};
	// ディゾルブの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DissolveSetting* dissolveSetting_ = nullptr;
};
