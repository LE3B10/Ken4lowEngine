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
private:
	// デフォルトコンストラクタ  
	LightManager() = default;
	// デフォルトデストラクタ  
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

	static LightManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// ImGuiを描画
	void DrawImGui();

	// ライト情報をシェーダーにバインド
	void BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2);

private: /// ---------- メンバ関数 ---------- ///

	// パンクチュアルライトの生成
	void CreatePunctualLight();

	// パンクチュアルライトの更新
	void UpdatePunctualLight();

	// デバッグ用ライトギズモ描画
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

	// コピーコンストラクタ
	LightManager(const LightManager&) = delete;
	// コピー代入演算子
	LightManager& operator=(const LightManager&) = delete;
	// ムーブコンストラクタ
	LightManager(LightManager&&) = delete;
	// ムーブ代入演算子
	LightManager& operator=(LightManager&&) = delete;

};

