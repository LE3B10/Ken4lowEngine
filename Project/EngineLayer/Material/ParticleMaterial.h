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

	/// <summary>
	/// コンストラクタ
	/// </summary>
	ParticleMaterial() = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// 指定したルートパラメーターのインデックスに基づいてパイプラインを設定する、オブジェクトを変更しない const メンバー関数。
	/// </summary>
	/// <param name="rootParameterIndex">設定対象のルートパラメーターのインデックス。省略した場合の既定値は 0</param>
	void SetPipeline(UINT rootParameterIndex = 0) const;

	/// <summary>
	/// ImGuiの描画
	/// </summary>
	void DrawImGui();

public: /// ---------- メンバ変数 ---------- ///

	MaterialCBData* materialData_ = nullptr; // マテリアルデータ
	ComPtr<ID3D12Resource> materialResource_; // マテリアルリソース

	// テクスチャ系データ
	std::string textureFilePath;			 // テクスチャファイルパス
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{}; // GPUハンドル

};

