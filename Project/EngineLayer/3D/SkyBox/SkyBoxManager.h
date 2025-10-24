#pragma once
#include "DX12Include.h"


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　スカイボックスを管理するクラス
/// -------------------------------------------------------------
class SkyBoxManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static SkyBoxManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void SetRenderSetting();

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// PSOを生成
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	BlendMode blendMode_ = BlendMode::kBlendModeNone;

	ComPtr <ID3D12PipelineState> graphicsPipelineState_;
	ComPtr <ID3DBlob> signatureBlob_;
	ComPtr <ID3DBlob> errorBlob_;
	ComPtr <ID3D12RootSignature> rootSignature_;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};
};

