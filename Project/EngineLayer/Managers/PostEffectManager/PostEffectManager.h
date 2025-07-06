#pragma once
#include "DX12Include.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"

#include <algorithm>
#include <string>
#include <numbers>
#include <unordered_map>
#include <functional>

#include <IPostEffect.h>
#include "PostEffectPipelineBuilder.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Camera;


/// -------------------------------------------------------------
///				　ポストエフェクトを管理するクラス
/// -------------------------------------------------------------
class PostEffectManager
{
private: /// ---------- 構造体 ---------- ///

	// ポストエフェクトのエントリ
	struct EffectEntry
	{
		std::function<std::unique_ptr<IPostEffect>()> creator;
		bool enabled;
		int order;           // 適用順
		std::string category; // 任意（例："Visual", "Debug", "Color"など）
	};

public:

	struct RenderTarget
	{
		ComPtr<ID3D12Resource> resource = nullptr; // レンダーテクスチャリソース
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {}; // RTVハンドル
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON; // リソース状態
		uint32_t srvIndex = 0; // SRVインデックス
		uint32_t uavIndex = 0; // UAVインデックス
		Vector4 clearColor = { 0.08f, 0.08f, 0.18f, 1.0f }; // クリアカラー
	};

public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static PostEffectManager* GetInstance();

	// 初期化処理
	void Initialieze(DirectXCommon* dxCommon);

	// 更新処理
	void Update();

	// 描画開始処理
	void BeginDraw();

	// 描画終了処理
	void EndDraw();

	// ポストエフェクトの描画適用処理
	void RenderPostEffect();

	// ImGuiの描画
	void ImGuiRender();

	void EnableEffect(const std::string& effectName) { effectEnableFlags_[effectName] = true; } // エフェクトを有効化
	void DisableEffect(const std::string& effectName) { effectEnableFlags_[effectName] = false; }  // エフェクトを無効化

private: /// ---------- メンバ関数 ---------- ///

	// レンダーテクスチャリソースの生成
	ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	// 深度バッファリソースの生成
	ComPtr<ID3D12Resource> CreateDepthBufferResource(uint32_t width, uint32_t height);

private: /// ---------- メンバ関数 ---------- ///

	// RTVとSRVの確保
	void AllocateRTV_DSV_SRV_UAV();

	// ビューポート矩形とシザリング矩形の設定
	void SetViewportAndScissorRect();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// エフェクトを管理するクラス
	std::unordered_map<std::string, std::unique_ptr<IPostEffect>> postEffects_;
	std::unique_ptr<PostEffectPipelineBuilder> pipelineBuilder_ = nullptr;

	// エフェクトを有効にするかどうかのフラグ
	std::unordered_map<std::string, bool> effectEnabled_;
	std::unordered_map<std::string, bool> effectEnableFlags_; // エフェクトのON/OFFフラグ

	// ポストエフェクトの適用順（名前と順序番号）
	std::vector<std::pair<std::string, int>> effectOrder_;

	// ポストエフェクトのカテゴリ分類（名前 → カテゴリ名）
	std::unordered_map<std::string, std::string> effectCategory_;

	// レンダーテクスチャのクリアカラー
	const Vector4 kRenderTextureClearColor_ = { 0.08f, 0.08f, 0.18f, 1.0f }; // 分かりやすいように一旦赤色にする

	ComPtr <ID3DBlob> signatureBlob_;
	ComPtr <ID3DBlob> errorBlob_;

	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	// DSVのハンドル
	ComPtr<ID3D12Resource> depthResource_;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	D3D12_RESOURCE_STATES depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	uint32_t dsvSrvIndex_ = 0;

	static constexpr int kPostRTCount = 1; // ポストエフェクト用のレンダーテクスチャ数
	std::vector<RenderTarget> renderTargets_; // レンダーテクスチャのリスト

private: /// ---------- コピー禁止 ---------- ///

	PostEffectManager() = default;
	~PostEffectManager() = default;
	PostEffectManager(const PostEffectManager&) = delete;
	const PostEffectManager& operator=(const PostEffectManager&) = delete;
};
