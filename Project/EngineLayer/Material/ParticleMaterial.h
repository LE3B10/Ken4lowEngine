#pragma once
#include <DX12Include.h>
#include "Vector4.h"
#include "Matrix4x4.h"


/// -------------------------------------------------------------
///				　パーティクル用マテリアルクラス
/// -------------------------------------------------------------
class ParticleMaterial
{
public: /// ---------- 構造体 ---------- ///

	// マテリアルデータ 定数バッファで送るデータ
	struct MaterialCBData
	{
		Vector4 color;			// 色
		Matrix4x4 uvTransform;  // UV変換行列
		float padding[3];		// パディング
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	ParticleMaterial() = default;
	
	// 初期化処理
	void Initialize();
	
	// 更新処理
	void Update();
	
	// パイプラインの設定
	void SetPipeline(UINT rootParameterIndex = 0) const;
	
	// ImGuiの描画
	void DrawImGui();

public: /// ---------- メンバ変数 ---------- ///

	MaterialCBData* materialData_ = nullptr; // マテリアルデータ
	ComPtr<ID3D12Resource> materialResource_; // マテリアルリソース

	// テクスチャ系データ
	std::string textureFilePath;			 // テクスチャファイルパス
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{}; // GPUハンドル

};

