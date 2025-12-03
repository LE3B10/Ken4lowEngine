#pragma once
#include "DX12Include.h"
#include <string>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///				　ポストエフェクトパイプラインビルダー
/// -------------------------------------------------------------
class PostEffectPipelineBuilder
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DirectXCommon を用いて DirectX に関する共通リソースや状態を初期化します。
	/// </summary>
	/// <param name="dxCommon">初期化対象の DirectXCommon オブジェクトへのポインタ。DirectX の共通リソースやコンテキストへのアクセスに使用されます。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// PostEffectPipelineBuilder の終了処理を行います。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 新しい ID3D12RootSignature を作成し、その参照を保持する ComPtr を返します。
	/// </summary>
	/// <returns>作成された ID3D12RootSignature を指す Microsoft::WRL::ComPtr<ID3D12RootSignature>。</returns>
	ComPtr<ID3D12RootSignature> CreateRootSignature();

	/// <summary>
	/// ピクセルシェーダーとルートシグネチャを使用してグラフィックス用の ID3D12PipelineState オブジェクトを作成します。
	/// </summary>
	/// <param name="pixelShaderPath">ピクセルシェーダーのファイルパス（ワイド文字列、std::wstring）。シェーダーバイナリまたはコンパイル済みファイルの場所を指定します。</param>
	/// <param name="rootSignature">作成するパイプラインで使用する ID3D12RootSignature へのポインタ。nullptr を渡すと正しく動作しない可能性があります。</param>
	/// <param name="enableDepth">深度テストおよび深度書き込みを有効にするかどうかを示すフラグ。既定値は false（深度無効）。</param>
	/// <returns>作成された ID3D12PipelineState を保持する ComPtr。作成に失敗した場合は空の ComPtr を返す可能性があります。</returns>
	ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(const std::wstring& pixelShaderPath, ID3D12RootSignature* rootSignature, bool enableDepth = false);

	/// <summary>
	/// コンピュートパイプラインで使用するルートシグネチャ (ID3D12RootSignature) を作成して ComPtr として返します。
	/// </summary>
	/// <returns>作成された ID3D12RootSignature を保持する ComPtr。作成に失敗した場合は空の ComPtr を返すことがあります。</returns>
	ComPtr<ID3D12RootSignature> CreateComputeRootSignature();

	/// <summary>
	/// コンピュートパイプラインステートの生成
	/// </summary>
	/// <param name="csPath">コンピュートシェーダーのパス</param>
	/// <param name="rootSignature">コンピュートルートシグネチャ</param>
	/// <returns></returns>
	ComPtr<ID3D12PipelineState> CreateComputePipeline(const std::wstring& csPath, ID3D12RootSignature* rootSignature);

	/// <summary>
	/// コピー操作に使用するパイプラインを構築します。
	/// </summary>
	void BuildCopyPipeline();

	/// <summary>
	/// コピー用の ID3D12RootSignature を指す ComPtr<ID3D12RootSignature> を返す const メンバー関数。
	/// </summary>
	/// <returns>保持している copyRootSignature_ の ComPtr をコピーして返します。設定されていない場合は空の ComPtr（null）を返します。</returns>
	ComPtr<ID3D12RootSignature> GetCopyRootSignature() const { return copyRootSignature_; }

	/// <summary>
	/// コピー操作で使用するパイプライン状態を表す ComPtr<ID3D12PipelineState> を返します。メソッドは const でオブジェクトを変更しません。
	/// </summary>
	/// <returns>ComPtr<ID3D12PipelineState>：内部の copyPipelineState_ をコピーして返すスマートポインター。コピー用のパイプライン状態オブジェクトへの参照を管理します。</returns>
	ComPtr<ID3D12PipelineState> GetCopyPipelineState() const { return copyPipelineState_; }

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// 保管先
	ComPtr<ID3D12RootSignature> copyRootSignature_; // コピー用ルートシグネチャ
	ComPtr<ID3D12PipelineState> copyPipelineState_; // コピー用パイプラインステート
};

