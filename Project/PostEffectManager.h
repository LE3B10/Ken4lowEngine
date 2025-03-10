#pragma once
#include "DX12Include.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <string>
#include <numbers>
#include <unordered_map>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　ポストエフェクトを管理するクラス
/// -------------------------------------------------------------
class PostEffectManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static PostEffectManager* GetInstance();

	// 初期化処理
	void Initialieze(DirectXCommon* dxCommon);

	// 描画前処理
	void BeginDraw();

	// 描画処理
	void Draw(const std::string& effectName);

public: /// ---------- セッター ---------- ///

	// バリア処理
	void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);


private: /// ---------- メンバ関数 ---------- ///

	// レンダーテクスチャの初期化処理
	void InitializeRenderTexture();

	// レンダーテクスチャリソースの生成
	void CreateRenderTextureResource(ComPtr<ID3D12Resource>& resource, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	// ルートシグネチャの生成
	void CreateRootSignature(const std::string& effectName);

	// PSOを生成
	void CreatePSO(const std::string& effectName);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	// レンダーテクスチャリソース
	ComPtr<ID3D12Resource> renderTextureResource_;

	// レンダーテクスチャのRTVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRTVHandle_;

	// レンダーテクスチャのクリアカラー
	const Vector4 kRenderTextureClearColor_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // 分かりやすいように一旦赤色にする

	ComPtr <ID3DBlob> signatureBlob_;
	ComPtr <ID3DBlob> errorBlob_;

	// ルートシグネチャ
	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> rootSignatures_;

	// パイプラインスレート
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> graphicsPipelineStates_;

	// シェーダーリソースビューのインデックス
	uint32_t rtvSRVIndex = 0;
	uint32_t dsvSRVIndex = 0;

private: /// ---------- コピー禁止 ---------- ///

	PostEffectManager() = default;
	~PostEffectManager() = default;
	PostEffectManager(const PostEffectManager&) = delete;
	const PostEffectManager& operator=(const PostEffectManager&) = delete;
};
