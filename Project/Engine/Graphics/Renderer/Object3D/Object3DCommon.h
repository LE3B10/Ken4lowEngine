#pragma once
#include "DX12Include.h"
#include "LightManager.h"
#include "Camera.h"
#include <BlendModeType.h>

#include <memory>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;

	/// -------------------------------------------------------------
	///				　	オブジェクト3Dの共通クラス
	/// -------------------------------------------------------------
	class Object3DCommon
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// Object3DCommon のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>Object3DCommon の唯一のインスタンス。</returns>
		static Object3DCommon* GetInstance();

		/// <summary>
		/// 3Dオブジェクト描画用の共通初期化処理を行います。<br/>
		/// ・DirectXCommon の保持<br/>
		/// ・デバッグカメラフラグの初期化（false）<br/>
		/// ・ルートシグネチャ／グラフィックスパイプラインステート(PSO)の生成<br/>
		/// ・LightManager の初期化<br/>
		/// をまとめて行います。
		/// </summary>
		/// <param name="dxCommon">D3D12 デバイスやコマンドリストを管理する DirectXCommon へのポインタ。</param>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// Object3DCommon の終了処理を行います。<br/>
		/// ・LightManager の終了処理<br/>
		/// </summary>
		void Finalize();

		/// <summary>
		/// ImGui によるデバッグ描画を行います。<br/>
		/// 現状は空実装ですが、将来的に共通パラメータの GUI 調整などを行うためのフックです。  
		/// </summary>
		void DrawImGui();

	public: /// ---------- 設定 ---------- ///

		/// <summary>
		/// 3Dオブジェクト描画に必要な共通レンダリング設定をコマンドリストに反映します。<br/>
		/// ・ルートシグネチャのセット<br/>
		/// ・グラフィックスパイプラインステートのセット<br/>
		/// ・プリミティブトポロジを三角形リストに設定<br/>
		/// ・LightManager によるライティング用 SRV／CBV のバインド（BindPunctualLights）<br/>
		/// などを行い、各 Object3D インスタンス描画の前に呼び出される想定です。
		/// </summary>
		void SetRenderSetting();

		/// <summary>
		/// シャドウマップのレンダリング設定を設定します。
		/// </summary>
		void SetShadowMapRenderSetting();

	private: /// ---------- 内部メンバ関数 ---------- ///

		/// <summary>
		/// Object3D 共通描画用のルートシグネチャを生成します。<br/>
		/// ・マテリアル CBV (b0)<br/>
		/// ・TransformationMatrix CBV (b0 VS)<br/>
		/// ・テクスチャ用 SRV テーブル (t0)<br/>
		/// ・カメラ CBV (b1)<br/>
		/// ・キューブマップ SRV テーブル (t1)<br/>
		/// ・ライト数 CBV (b2)<br/>
		/// ・ライト配列 SRV テーブル (t2)<br/>
		/// ・ディゾルブ用 CBV (b3) / SRV (t3)<br/>
		/// などのルートパラメータと、ピクセルシェーダ用サンプラを設定し、<br/>
		/// D3D12SerializeRootSignature → CreateRootSignature によって生成します。
		/// </summary>
		void CreateRootSignature();

		/// <summary>
		/// Object3D 共通描画用のグラフィックスパイプラインステート(PSO)を生成します。<br/>
		/// ・入力レイアウト（POSITION / TEXCOORD / NORMAL）<br/>
		/// ・BlendState（BlendStateFactory から取得）<br/>
		/// ・RasterizerState（カリング無効など）<br/>
		/// ・DepthStencilState（Depth 有効・LessEqual）<br/>
		/// ・頂点シェーダ／ピクセルシェーダのコンパイル＆設定<br/>
		/// ・RenderTarget / DSV フォーマット、プリミティブトポロジ種別 など<br/>
		/// をまとめて設定し、ID3D12PipelineState を生成します。
		/// </summary>
		void CreatePSO();

		void CreateShadowRootSignature();

		void CreateShadowPSO();

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;

		BlendMode blendMode_ = BlendMode::kBlendModeNone;

		ComPtr <ID3D12PipelineState> graphicsPipelineState_;
		ComPtr <ID3DBlob> signatureBlob_;
		ComPtr <ID3DBlob> errorBlob_;
		ComPtr <ID3D12RootSignature> rootSignature_;

		// 影用
		ComPtr<ID3D12PipelineState> shadowPipelineState_;
		ComPtr<ID3D12RootSignature> shadowRootSignature_;
		ComPtr<ID3DBlob> shadowSignatureBlob_;
		ComPtr<ID3DBlob> shadowErrorBlob_;

		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};

	private: /// ---------- コピー禁止 ---------- ///

		Object3DCommon() = default;
		~Object3DCommon() = default;
		Object3DCommon(const Object3DCommon&) = delete;
		Object3DCommon& operator=(const Object3DCommon&) = delete;
	};


} // namespace Ken4lowEngine
