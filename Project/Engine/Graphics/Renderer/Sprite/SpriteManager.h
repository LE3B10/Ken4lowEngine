#pragma once
#include "DX12Include.h"

namespace Ken4lowEngine
{


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　	スプライトを管理するクラス
/// -------------------------------------------------------------
class SpriteManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// SpriteManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>SpriteManager の唯一のインスタンス。</returns>
	static SpriteManager* GetInstance();

	/// <summary>
	/// スプライト描画に必要な共通リソースを初期化します。<br/>
	/// ・DirectXCommon のポインタを保持<br/>
	/// ・ルートシグネチャと 2 種類のグラフィックスパイプラインステート(PSO)を生成<br/>
	/// を行います。
	/// </summary>
	/// <param name="dxCommon">D3D12 デバイスやコマンドリストを管理する DirectXCommon。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// スプライト描画に必要な共通リソースを解放します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 背景スプライトを描画する前に呼び出す共通設定です。<br/>
	/// ・ルートシグネチャをセット<br/>
	/// ・背景用 PSO（graphicsPipelineState_Background_）をセット<br/>
	/// ・プリミティブトポロジを三角形リストに設定<br/>
	/// などを行い、その後各 Sprite クラスから Draw される想定です。
	/// </summary>
	void SetRenderSetting_Background();

	/// <summary>
	/// UI スプライトを描画する前に呼び出す共通設定です。<br/>
	/// ・ルートシグネチャをセット<br/>
	/// ・UI 用 PSO（graphicsPipelineState_UI_）をセット<br/>
	/// ・プリミティブトポロジを三角形リストに設定<br/>
	/// などを行い、Z 書き込み無しの UI 用スプライト描画に使用します。
	/// </summary>
	void SetRenderSetting_UI();

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// スプライト描画用のルートシグネチャを生成します。<br/>
	/// ルートパラメータ構成は以下の通りです：<br/>
	/// ・b0 : マテリアル用 CBV（色・UV など）<br/>
	/// ・b1 : リロード進捗用 CBV（ReloadProgress）<br/>
	/// ・b0 : TransformationMatrix 用 CBV（VS 用）<br/>
	/// ・t0 : テクスチャ SRV（ディスクリプタテーブル）<br/>
	/// また、線形フィルタ／WRAP の静的サンプラ s0 を 1 つ登録します。
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// スプライト描画用のグラフィックスパイプラインステート(PSO)を生成します。<br/>
	/// ・入力レイアウト：POSITION(float4) / TEXCOORD(float2)<br/>
	/// ・ブレンドステート：BlendStateFactory から blendMode_ をもとに取得<br/>
	/// ・ラスタライザ：塗りつぶし、カリング無し<br/>
	/// ・VS：Sprite.VS.hlsl、PS：Sprite.PS.hlsl をコンパイルして設定<br/>
	/// ・RTV フォーマット：R8G8B8A8_UNORM_SRGB<br/>
	/// ・DSV フォーマット：D24_UNORM_S8_UINT<br/>
	/// などを共通で設定し、<br/>
	/// ・背景用（DepthWrite 有り）graphicsPipelineState_Background_<br/>
	/// ・UI 用（DepthWrite 無し）graphicsPipelineState_UI_<br/>
	/// の 2 種類の PSO を生成します。
	/// </summary>
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	BlendMode blendMode_ = BlendMode::kBlendModeNormal;

	// 2種類のPSO
	ComPtr<ID3D12PipelineState> graphicsPipelineState_Background_;
	ComPtr<ID3D12PipelineState> graphicsPipelineState_UI_;

	// シェーダ関連
	ComPtr <ID3DBlob> signatureBlob_; // シグネチャ用Blob
	ComPtr <ID3DBlob> errorBlob_;	  // エラーメッセージ用Blob

	// ルートシグネチャ
	ComPtr <ID3D12RootSignature> rootSignature_;

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>外部からの生成を禁止するためのプライベートコンストラクタ。</summary>
	SpriteManager() = default;
	/// <summary>デフォルトデストラクタ。</summary>
	~SpriteManager() = default;
	/// <summary>コピーコンストラクタは禁止。</summary>
	SpriteManager(const SpriteManager&) = delete;
	/// <summary>代入演算子は禁止。</summary>
	SpriteManager& operator=(const SpriteManager&) = delete;
};

} // namespace Ken4lowEngine
