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

	/// <summary>
	/// SkyBoxManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>SkyBoxManager の唯一のインスタンス。</returns>
	static SkyBoxManager* GetInstance();

	/// <summary>
	/// スカイボックス描画の初期化処理を行います。<br/>
	/// ・DirectXCommon の保持<br/>
	/// ・ルートシグネチャの生成<br/>
	/// ・スカイボックス用グラフィックスパイプラインステート(PSO)の生成<br/>
	/// などをまとめて行います。
	/// </summary>
	/// <param name="dxCommon">デバイスやコマンドリストを取得するための DirectXCommon へのポインタ。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// スカイボックス描画用の共通レンダリング設定をコマンドリストに反映します。<br/>
	/// </summary>
	void Finalize();

	/// <summary>
	/// スカイボックス描画用の共通レンダリング設定をコマンドリストに反映します。<br/>
	/// ・ルートシグネチャのセット<br/>
	/// ・グラフィックスパイプラインステート(PSO)のセット<br/>
	/// ・ブレンドモードの設定 などを行い、<br/>
	/// 実際のスカイボックスメッシュを描画する前に呼び出すことを想定しています。
	/// </summary>
	void SetRenderSetting();

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// スカイボックス用のルートシグネチャを生成します。<br/>
	/// ビュー・プロジェクション行列用の定数バッファや、<br/>
	/// キューブマップテクスチャ用 SRV のディスクリプタテーブルなどを設定します。
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// スカイボックス描画用のグラフィックスパイプラインステート(PSO)を生成します。<br/>
	/// ・入力レイアウト（位置などの頂点フォーマット）<br/>
	/// ・ラスタライザステート（カリング / 塗りつぶしモード）<br/>
	/// ・ブレンドステート（通常はブレンドなし）<br/>
	/// ・深度ステンシルステート（深度テスト ON / 書き込み OFF など）<br/>
	/// ・シェーダバイナリ（VS / PS）<br/>
	/// をまとめて設定し、ID3D12PipelineState を作成します。
	/// </summary>
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

