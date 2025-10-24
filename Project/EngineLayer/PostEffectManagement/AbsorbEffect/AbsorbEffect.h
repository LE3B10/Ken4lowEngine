#pragma once
#include "IPostEffect.h"


/// -------------------------------------------------------------
///					アブソーブエフェクトクラス
/// -------------------------------------------------------------
class AbsorbEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	/// ---------- 吸収エフェクトの設定構造体 ---------- ///
	struct AbsorbSetting
	{
		float time;		  // 経過時間
		float strength;   // 吸収の強さ
		float padding[2]; // アライメント調整
	};

	/// ---------- アブソーブのパラメータ構造体 ---------- ///
	struct AbsorbParams
	{
		float timestepPerFrame = 1.0f / 12.0f; // フレームごとの時間ステップ
		float loopDuration = 1.0f;			   // ループの持続時間
		float strengthMin = 0.0f;			   // 吸収の最小強度
		float strengthMax = 5.0f;			   // 吸収の最大強度
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// アブソーブエフェクトの初期化処理
	/// </summary>
	/// <param name="dxCommon">DirectX共通</param>
	/// <param name="builder">ポストエフェクトパイプラインビルダー</param>
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	/// <summary>
	///	アブソーブエフェクトの更新処理
	/// </summary>
	void Update() override;

	/// <summary>
	/// アブソーブエフェクトの適用処理
	/// </summary>
	/// <param name="commandList">コマンドリスト</param>
	/// <param name="srvIndex">SRVインデックス</param>
	/// <param name="uavIndex">UAVインデックス</param>
	/// <param name="dsvIndex">DSVインデックス</param>
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

	/// <summary>
	/// アブソーブエフェクトのImGui描画処理
	/// </summary>
	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_ = nullptr;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// 吸収の設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

	// 吸収設定のマッピング先
	AbsorbSetting* absorbSetting_ = nullptr;

	// アブソーブのパラメータ
	AbsorbParams absorbParams_ = {};
};

