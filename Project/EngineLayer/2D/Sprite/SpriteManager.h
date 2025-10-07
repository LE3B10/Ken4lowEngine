#pragma once
#include "DX12Include.h"


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　	スプライトを管理するクラス
/// -------------------------------------------------------------
class SpriteManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static SpriteManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 更新処理
	void Update();

	// 背景用の共通描画設定（Depth書き込み有）
	void SetRenderSetting_Background();

	// UI用の共通描画設定（Depth書き込み無）
	void SetRenderSetting_UI();

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// PSOを生成
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	BlendMode blendMode_ = BlendMode::kBlendModeNormal;

	// 2種類のPSO
	ComPtr<ID3D12PipelineState> graphicsPipelineState_Background_;
	ComPtr<ID3D12PipelineState> graphicsPipelineState_UI_;

	ComPtr <ID3DBlob> signatureBlob_;
	ComPtr <ID3DBlob> errorBlob_;
	ComPtr <ID3D12RootSignature> rootSignature_;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};

private: /// ---------- コピー禁止 ---------- ///

	SpriteManager() = default;
	~SpriteManager() = default;
	SpriteManager(const SpriteManager&) = delete;
	SpriteManager& operator=(const SpriteManager&) = delete;

};
