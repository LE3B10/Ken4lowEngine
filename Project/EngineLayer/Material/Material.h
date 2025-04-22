#pragma once
#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include <cstdint>


/// -------------------------------------------------------------
///					　	マテリアルクラス
/// -------------------------------------------------------------
class Material
{
public: /// ---------- 構造体 ---------- ///

	// マテリアルデータ 定数バッファで送るデータ
	struct MaterialCBData
	{
		Vector4 color;			// 色
		bool enableLighting;	// 平行光源の有無
		float padding[3];		// パディング
		Matrix4x4 uvTransform;  // UV変換行列
		float shininess;		// シェーディングの強さ
	};

public: /// ---------- メンバ変数 ---------- ///

	// テクスチャ系データ
	std::string textureFilePath;			 // テクスチャファイルパス
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{}; // GPUハンドル

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクト
	Material() = default;

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// パイプラインの設定
	void SetPipeline(UINT rootParameterIndex = 0) const;

	// ImGuiの描画
	void DrawImGui();

public: /// ---------- ゲッタ ---------- ///

	// マテリアルリソースを取得
	ComPtr<ID3D12Resource> GetMaterialResource() { return materialResource_; }

	// マテリアルデータを取得
	MaterialCBData* GetMaterialData() { return materialData_; }

public: /// ---------- セッタ ---------- ///

	// 色を設定
	void SetColor(const Vector4& color) { materialData_->color = color; }

	// 平行光源の有無を設定
	void SetEnableLighting(bool enable) { materialData_->enableLighting = enable; }

	// シェーディングの強さを設定
	void SetShininess(float shininess) { materialData_->shininess = shininess; }

	// UV変換行列を設定
	void SetUVTransform(const Matrix4x4& uvTransform) { materialData_->uvTransform = uvTransform; }

private: /// ---------- メンバ変数 ---------- ///

	// マテリアル用のリソース
	ComPtr<ID3D12Resource> materialResource_{};
	// マテリアルデータ
	MaterialCBData* materialData_{};

};