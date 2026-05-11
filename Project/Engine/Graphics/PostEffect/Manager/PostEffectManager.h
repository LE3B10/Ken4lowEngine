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

namespace Ken4lowEngine
{

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
		std::function<std::unique_ptr<IPostEffect>()> creator; // エフェクト生成用のファクトリー関数
		bool enabled; 	   // 初期有効フラグ
		int order;           // 適用順
		std::string category; // カテゴリ名
	};

public: /// ---------- テンプレート ---------- ///

	// レンダーターゲットを表す構造体
	struct RenderTarget
	{
		ComPtr<ID3D12Resource> resource = nullptr;				   // レンダーテクスチャリソース
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};				   // RTVハンドル
		const wchar_t* debugName = L"PostEffectRenderTarget";		   // DebugLayerでUnnamed Resourceにしないための名前
		D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON; // 実リソースの現在状態を保持して固定beforeのBarrierを避ける
		uint32_t rtvIndex = UINT32_MAX;							   // RTVインデックス
		uint32_t srvIndex = UINT32_MAX;							   // SRVインデックス
		uint32_t uavIndex = UINT32_MAX;							   // UAVインデックス
		uint32_t srvIndexOnUavHeap = UINT32_MAX;				   // CS用に、UAVヒープ上へ複製したSRV
		Vector4 clearColor = { 0.08f, 0.08f, 0.18f, 1.0f };		   // クリアカラー
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// PostEffectManager のシングルトンインスタンスへのポインタを返します。
	/// </summary>
	/// <returns>PostEffectManager へのポインタ。シングルトンとして管理されるインスタンスを指します。</returns>
	static PostEffectManager* GetInstance();

	/// <summary>
	/// DirectXCommon オブジェクトを使用して初期化処理を行う関数。
	/// </summary>
	/// <param name="dxCommon">初期化対象の DirectXCommon オブジェクトへのポインター。関数はこのオブジェクトを用いて必要な初期化を行います。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// PostEffectManager の終了処理を行う関数。
	/// </summary>
	void Finalize();

	/// <summary>
	/// オブジェクトやシステムの状態を更新する関数。
	/// </summary>
	void Update();

	/// <summary>
	/// 描画操作の開始を宣言します。描画コマンドを発行する前に呼び出して、描画コンテキストを準備します。
	/// </summary>
	void BeginDraw();

	/// <summary>
	/// 現在の描画操作を終了します。フレームの描画を完了し、必要に応じてバッファの入れ替えや後処理を行います。
	/// </summary>
	void EndDraw();

	/// <summary>
	/// 
	/// </summary>
	/// <param name="width"></param>
	/// <param name="height"></param>
	void Resize(uint32_t width, uint32_t height);

	/// <summary>
	/// ポストプロセス（後処理）エフェクトをレンダリングします。
	/// </summary>
	void RenderPostEffect();

	/// <summary>
	/// ポストエフェクト済みのゲーム画面へ2D描画を重ねるためのRTVをバインドします。
	/// </summary>
	void BeginGameRenderTargetOverlay();

	/// <summary>
	/// Main Viewport の ImGui::Image から読めるようゲーム画面をSRV状態へ戻します。
	/// </summary>
	void EndGameRenderTargetOverlay();

	void BindSceneRenderTarget();

	/// <summary>
	/// ImGui の描画をレンダリングします。
	/// </summary>
	void ImGuiRender(bool* pOpen = nullptr);

	/// <summary>
	/// 指定された名前のエフェクトを有効にします。内部の effectEnableFlags_ マップの該当エントリを true に設定します。
	/// </summary>
	/// <param name="effectName">有効にするエフェクトの名前。</param>
	void EnableEffect(const std::string& effectName) { effectEnableFlags_[effectName] = true; }

	/// <summary>
	/// 指定された名前のエフェクトを無効にします。内部の effectEnableFlags_ マップの該当エントリを false に設定します。
	/// </summary>
	/// <param name="effectName">無効にするエフェクトの名前。</param>
	void DisableEffect(const std::string& effectName) { effectEnableFlags_[effectName] = false; }

	/// <summary>
	/// 指定された名前のポストエフェクト(IPostEffect)を取得します。見つからなければ nullptr を返します。
	/// </summary>
	IPostEffect* GetEffect(const std::string& effectName);

	/// <summary>
	/// Main Viewport に表示する GameRenderTarget の SRV index を取得します。
	/// </summary>
	uint32_t GetGameRenderTargetSrvIndex() const;

	/// <summary>
	/// ImGui::Image に渡す GameRenderTarget の GPU SRV ハンドルを取得します。
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGameRenderTargetSrvHandleGPU() const;

	uint32_t GetGameRenderTargetWidth() const { return renderTargetWidth_; }
	uint32_t GetGameRenderTargetHeight() const { return renderTargetHeight_; }

	/// <summary>
	/// Main Viewport の表示サイズに合わせてリサイズしたい場合の入口です。
	/// </summary>
	void RequestGameRenderTargetResize(uint32_t width, uint32_t height);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定した幅・高さ・ピクセル形式・クリアカラーでレンダー用テクスチャ（リソース）を作成して返します。
	/// </summary>
	/// <param name="width">テクスチャの幅（ピクセル単位）。</param>
	/// <param name="height">テクスチャの高さ（ピクセル単位）。</param>
	/// <param name="format">テクスチャのピクセルフォーマットを指定する DXGI_FORMAT。</param>
	/// <param name="clearColor">リソース作成時に使用するクリアカラー（RGBA を表す Vector4）。</param>
	/// <returns>作成された ID3D12Resource を保持する ComPtr。レンダーターゲットとして使用できるテクスチャリソースを指し、作成に失敗した場合は空の ComPtr を返す可能性があります。</returns>
	ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	/// <summary>
	/// 指定した幅と高さで深度バッファー用の ID3D12Resource を作成します。
	/// </summary>
	/// <param name="width">深度バッファーの幅（ピクセル単位）。</param>
	/// <param name="height">深度バッファーの高さ（ピクセル単位）。</param>
	/// <returns>作成された深度バッファーを参照する ComPtr<ID3D12Resource>。作成に失敗した場合は空の ComPtr を返すことがあります。</returns>
	ComPtr<ID3D12Resource> CreateDepthBufferResource(uint32_t width, uint32_t height);

	/// <summary>
	/// RenderTarget の現在状態を見て必要なときだけ ResourceBarrier を発行します。
	/// </summary>
	void TransitionTo(RenderTarget& renderTarget, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// RTV、DSV、SRV、および UAV 用のディスクリプタ（ハンドル）を割り当てます。
	/// </summary>
	void AllocateRTV_DSV_SRV_UAV();

	/// <summary>
	/// ビューポートとシザー矩形（切り抜き矩形）を設定します。
	/// </summary>
	void SetViewportAndScissorRect(uint32_t width, uint32_t height);

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// カメラ
	Camera* camera_ = nullptr;

	// エフェクトを管理するクラス
	std::unordered_map<std::string, std::unique_ptr<IPostEffect>> postEffects_; // ポストエフェクトのリスト
	std::unique_ptr<PostEffectPipelineBuilder> pipelineBuilder_ = nullptr;		// ポストエフェクトパイプラインビルダー

	// エフェクトを有効にするかどうかのフラグ
	std::unordered_map<std::string, bool> effectEnabled_;	  // ImGui用のエフェクトON/OFFフラグ
	std::unordered_map<std::string, bool> effectEnableFlags_; // エフェクトのON/OFFフラグ

	// ポストエフェクトの適用順（名前と順序番号）
	std::vector<std::pair<std::string, int>> effectOrder_;

	// ポストエフェクトのカテゴリ分類（名前 → カテゴリ名）
	std::unordered_map<std::string, std::string> effectCategory_;

	// GameRenderTargetの初期サイズはエディタ中央表示用に16:9固定から開始する
	static constexpr uint32_t kInitialGameRenderTargetWidth_ = 1280;
	static constexpr uint32_t kInitialGameRenderTargetHeight_ = 720;

	// レンダーテクスチャのクリアカラー
	const Vector4 kRenderTextureClearColor_ = { 0.08f, 0.08f, 0.18f, 1.0f }; // 分かりやすいように一旦赤色にする

	// シェーダーバイナリ
	ComPtr <ID3DBlob> signatureBlob_; // ルートシグネチャ用
	ComPtr <ID3DBlob> errorBlob_;	  // エラーメッセージ用

	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{}; // ビューポート矩形
	D3D12_RECT scissorRect{};  // シザリング矩形

	// DSVのハンドル
	ComPtr<ID3D12Resource> depthResource_;		// 深度バッファリソース
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {}; // DSVハンドル
	D3D12_RESOURCE_STATES depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE; // 深度バッファのリソース状態
	uint32_t dsvSrvIndex_ = UINT32_MAX; // 深度バッファのSRVインデックス

	static constexpr int kPostRTCount = 1; // ポストエフェクト用のレンダーテクスチャ数
	std::vector<RenderTarget> renderTargets_; // レンダーテクスチャのリスト
	uint32_t renderTargetWidth_ = kInitialGameRenderTargetWidth_; // Main Viewportへ表示するGameRenderTarget幅
	uint32_t renderTargetHeight_ = kInitialGameRenderTargetHeight_; // Main Viewportへ表示するGameRenderTarget高さ

	// 深度ステンシルビューのインデックス
	uint32_t depthDsvIndex_ = UINT32_MAX;

private: /// ---------- コピー禁止 ---------- ///

	PostEffectManager() = default;
	~PostEffectManager() = default;
	PostEffectManager(const PostEffectManager&) = delete;
	const PostEffectManager& operator=(const PostEffectManager&) = delete;
};

} // namespace Ken4lowEngine
