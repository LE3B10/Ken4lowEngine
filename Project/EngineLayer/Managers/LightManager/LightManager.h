#pragma once
#include "DX12Include.h"
#include "Vector3.h" 
#include "Vector4.h"
#include "Matrix4x4.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　		ライトの管理クラス
/// -------------------------------------------------------------
class LightManager
{
private: /// ---------- コンストラクタ・デストラクタ ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
	/// シングルトンパターンとして利用します。
	/// </summary>
	LightManager() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~LightManager() = default;

public: /// ---------- 構造体 ---------- ///

	// パンクチュアルライトの構造体
	struct PunctualLightGPU
	{
		uint32_t lightType;		// ライトの種類（0：ライトなし、1：平行光源、2：点光源、3：スポットライト）
		Vector4 color;			// ライトの色 （全ライト共通）
		float intensity;		// 輝度 （全ライト共通）
		Vector3 position;		// ライトの位置 （点光源、スポットライト用）
		float radius;			// ライトの届く最大距離 （点光源用）
		float decay;			// 減衰率 （点光源、スポットライト用）
		Vector3 direction;		// スポットライトの方向 （平行光源、スポットライト用）
		float distance;			// ライトの届く最大距離 （スポットライト用）
		float cosFalloffStart;	// 開始角度 （スポットライト用）
		float cosAngle;			// スポットライトの余弦 （スポットライト用）
	};

	// ライト数CB
	struct LightInfo
	{
		uint32_t lightCount; // ライトの数
		float pad[3]; 		 // パディング
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// LightManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>LightManager の唯一のインスタンス。</returns>
	static LightManager* GetInstance();

	/// <summary>
	/// ライトマネージャの初期化処理。<br/>
	/// ・DirectXCommon の保持<br/>
	/// ・ライト数用 CBV の作成とマップ<br/>
	/// ・パンクチュアルライト用 Structured Buffer / SRV の準備（必要最小限のサイズ）<br/>
	/// を行います。
	/// </summary>
	/// <param name="dxCommon">DirectX12 共通クラスへのポインタ。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// ライトマネージャの終了処理。<br/>
	/// 内部で確保したリソースを解放します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// ImGui を用いたパンクチュアルライトの編集 UI を描画します。<br/>
	/// ・ライトの追加 / 全削除<br/>
	/// ・種類（None / Directional / Point / Spot）の切り替え<br/>
	/// ・色 / 輝度 / 位置 / 方向 / 半径 / 減衰 / 角度 の編集<br/>
	/// ・ライトごとの削除<br/>
	/// などを行います。
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// パンクチュアルライト情報をシェーダにバインドします。<br/>
	/// 内部で UpdatePunctualLight() を呼び出して GPU バッファと SRV を更新し、<br/>
	/// ・b2: LightInfo 用 CBV<br/>
	/// ・t2: パンクチュアルライト配列用 SRV<br/>
	/// をそれぞれルートパラメータに設定します。
	/// </summary>
	/// <param name="rootIndexCB_b2">ライト数 CBV をバインドするルートインデックス（b2）。</param>
	/// <param name="rootIndexSRV_t2">パンクチュアルライト SRV をバインドするルートインデックス（t2）。</param>
	void BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// パンクチュアルライト用リソースの生成処理。<br/>
	/// ・ライト数 CBV 用バッファの生成とマップ<br/>
	/// ・SRV 用インデックスの確保<br/>
	/// ・GPU バッファの最小確保と SRV 作成<br/>
	/// を行います。Initialize から一度だけ呼び出されます。
	/// </summary>
	void CreatePunctualLight();

	/// <summary>
	/// パンクチュアルライトの GPU バッファ更新処理。<br/>
	/// ・有効なライトだけを抽出・正規化して一時配列に詰める<br/>
	/// ・必要であればバッファを再確保（サイズ拡張）し、データを書き込む<br/>
	/// ・NumElements に応じて SRV を再作成（最低 1 要素）<br/>
	/// ・ライト数 CBV に有効ライト数を書き込み<br/>
	/// ・デバッグ用ライトギズモの描画<br/>
	/// を行います。
	/// </summary>
	void UpdatePunctualLight();

	/// <summary>
	/// デバッグ用のライトギズモを描画します。<br/>
	/// Wireframe クラスを用いて、ライトの種類に応じて：<br/>
	/// ・平行光源：原点付近に矢印線<br/>
	/// ・点光源：位置に小さな球＋到達半径の球（半透明）<br/>
	/// ・スポットライト：位置に球＋方向線<br/>
	/// を描画します。
	/// </summary>
	void DebugDrawLightGizmos();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	std::vector<PunctualLightGPU> punctualLights_; // GPUに送るライト情報

	// GPU送信用バッファとSRV
	Microsoft::WRL::ComPtr<ID3D12Resource> punctualBuffer_;
	uint32_t punctualBufferBytes_ = 0;
	uint32_t punctualSRVIndex_ = UINT32_MAX;
	bool     punctualSRVAllocated_ = false;
	uint32_t punctualType_ = 1; // 0=None, 1=Directional, 2=Point, 3=Spot

	Microsoft::WRL::ComPtr<ID3D12Resource> lightInfoResource_;
	LightInfo* lightInfoData_ = nullptr;

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>
	/// コピーコンストラクタは禁止。
	/// </summary>
	LightManager(const LightManager&) = delete;

	/// <summary>
	/// コピー代入演算子は禁止。
	/// </summary>
	LightManager& operator=(const LightManager&) = delete;

	/// <summary>
	/// ムーブコンストラクタは禁止。
	/// </summary>
	LightManager(LightManager&&) = delete;

	/// <summary>
	/// ムーブ代入演算子は禁止。
	/// </summary>
	LightManager& operator=(LightManager&&) = delete;
};

