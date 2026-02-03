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
	/// フレーム毎の更新処理を行います。<br/>
	/// isDebugCamera_ の状態に応じて、利用するカメラが切り替わります。<br/>
	/// ・通常カメラ使用時：<br/>
	///   - defaultCamera_ から View / Projection を取得し、ViewProjection を計算<br/>
	///   - defaultCamera_ に ViewProjection をセット<br/>
	///   - activeCameraPosition_ に defaultCamera_ の位置を保存<br/>
	/// ・デバッグカメラ使用時(_DEBUGビルドのみ)：<br/>
	///   - DebugCamera から ViewProjection と位置を取得<br/>
	///   - defaultCamera_ にデバッグ用 ViewProjection をセット<br/>
	///   - activeCameraPosition_ に DebugCamera の位置を保存<br/>
	/// </summary>
	void Update();

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
	/// 共通で使用する「デフォルトカメラ」を設定します。<br/>
	/// 通常描画時の View / Projection の取得元となります。
	/// </summary>
	/// <param name="defaultCamera">使用するカメラオブジェクトのポインタ。</param>
	void SetDefaultCamera(Camera* defaultCamera) { defaultCamera_ = defaultCamera; }

	/// <summary>
	/// デバッグカメラを使用するかどうかを設定します。 <br/>
	/// true の場合、_DEBUG ビルドでは DebugCamera の ViewProjection / 位置情報を使用し、<br/>
	/// リリースビルドでは通常カメラと同じ挙動になります。
	/// </summary>
	/// <param name="isDebugCamera">デバッグカメラを利用する場合 true。</param>
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

public:	/// ---------- 取得 ---------- ///

	/// <summary>
	/// 現在アクティブなカメラ（通常 or デバッグ）のワールド座標を取得します。<br/>
	/// Object3D のシェーダ定数バッファなどに「カメラ位置」を渡したいときに利用します。
	/// </summary>
	/// <returns>アクティブカメラの位置ベクトル。</returns>
	Vector3 GetActiveCameraPosition() const { return activeCameraPosition_; }

	/// <summary>
	/// デフォルトカメラを取得します。<br/>
	/// 通常カメラのパラメータに直接アクセスしたい場合に使用します。
	/// </summary>
	/// <returns>登録されているデフォルトカメラへのポインタ。</returns>
	Camera* GetDefaultCamera() const { return defaultCamera_; }

	/// <summary>
	/// デバッグカメラが有効かどうかを取得します。
	/// </summary>
	/// <returns>デバッグカメラ使用時 true。</returns>
	bool GetDebugCamera() const { return isDebugCamera_; }

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

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	// デフォルトカメラ
	Camera* defaultCamera_ = nullptr;

	BlendMode blendMode_ = BlendMode::kBlendModeNone;

	ComPtr <ID3D12PipelineState> graphicsPipelineState_;
	ComPtr <ID3DBlob> signatureBlob_;
	ComPtr <ID3DBlob> errorBlob_;
	ComPtr <ID3D12RootSignature> rootSignature_;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};

	// ビュー射影行列
	Matrix4x4 viewProjectionMatrix_;
	Matrix4x4 debugViewProjectionMatrix_;

	// アクティブなカメラの位置
	Vector3 activeCameraPosition_{ 0,0,0 };

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;

private: /// ---------- コピー禁止 ---------- ///

	Object3DCommon() = default;
	~Object3DCommon() = default;
	Object3DCommon(const Object3DCommon&) = delete;
	Object3DCommon& operator=(const Object3DCommon&) = delete;
};


} // namespace Ken4lowEngine
