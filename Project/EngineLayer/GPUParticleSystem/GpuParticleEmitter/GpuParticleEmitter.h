#pragma once
#include <string>
#include <Vector3.h>
#include <Vector4.h>

#include "GpuParticleType.h"		// GPUパーティクルの種類
#include "GpuParticleEmitterData.h" // エミッターのCBデータ
#include "BillboardMode.h" // ビルボードモード

/// -------------------------------------------------------------
///			　	GPUパーティクルエミッタークラス
/// -------------------------------------------------------------
class GpuParticleEmitter
{
public: /// ---------- 構造体 ---------- ///

	// エミッター情報構造体
	struct EmitterInfo
	{
		std::string textureFilePath; // テクスチャファイルパス
		float radius = 0.0f;          // 発生範囲
		uint32_t loopCount = 0;       // ループ発生時に1回で出す数
		float loopFrequency = 0.0f;   // ループ発生周期(秒)。0ならループしない

		// 描画パス用フィルタ
		uint32_t drawType = 0; // 描画タイプ

		GpuParticleType type = GpuParticleType::Default; // パーティクルの種類
		BillboardMode billboardMode = BillboardMode::Camera;
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// エミッターのコンストラクタ。<br/>
	/// 名前と基本設定（EmitterInfo）を受け取り、ループタイマーや累積発生数を初期化します。
	/// </summary>
	/// <param name="name">エミッター名（識別用のキー）</param>
	/// <param name="info">エミッターの基本設定</param>
	GpuParticleEmitter(const std::string& name, const EmitterInfo& info);

	/// <summary>
	/// 1 フレーム分の「追加バースト発生」をリクエストします。<br/>
	/// 引数 count を pendingBurstCount_ に加算するだけで、実際のエミットは BuildCB() を通じて行われます。<br/>
	/// Update() 側から複数回呼ばれても、そのフレーム内で合算されて利用されます。
	/// </summary>
	/// <param name="count">このフレームに追加で発生させたいパーティクル数</param>
	void RequestEmit(uint32_t count);

	/// <summary>
	/// 定期発射（ループ）とバースト発生をまとめて処理し、<br/>
	/// GPU に渡すエミッター用 CB データを構築します。<br/>
	/// ・loopFrequency / loopCount に基づいてループ発生数を計算し pendingBurstCount_ に加算<br/>
	/// ・pendingBurstCount_ が 0 なら何も書き込まず false を返す<br/>
	/// ・1 フレーム分の発生パラメータを GpuEmitterCBData に書き込み、pendingBurstCount_ を消費<br/>
	/// という流れで動作します。<br/>
	/// 戻り値 true のときだけ DispatchEmit() 側で Emit 用 Compute Shader を実行します。
	/// </summary>
	/// <param name="out">GPU に送るエミッター用 CB データの書き込み先</param>
	/// <param name="deltaTime">前フレームからの経過時間（秒）</param>
	/// <returns>今フレーム何かしらパーティクルを発生させる場合は true、それ以外は false</returns>
	bool BuildCB(GpuEmitterCBData& out, float deltaTime);

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// エミッターのワールド座標を設定します。<br/>
	/// GPU 側ではこの位置と radius をもとに、発生位置のランダムサンプリングなどを行います。
	/// </summary>
	/// <param name="position">新しいエミッター位置</param>
	void SetPosition(const Vector3& position) { position_ = position; }

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// エミッター名を取得します。
	/// </summary>
	/// <returns>コンストラクタで指定したエミッター名</returns>
	const std::string& GetName() const { return name_; }

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	const EmitterInfo& GetInfo() const { return info_; }

	// 描画に使うID（0なら type を返す）
	uint32_t GetDrawType() const
	{
		return (info_.drawType != 0) ? info_.drawType : static_cast<uint32_t>(info_.type);
	}

private: /// ---------- メンバ変数 ---------- ///

	std::string name_; // エミッター名
	EmitterInfo info_; // エミッター情報

	Vector3 position_{ 0.0f, 0.0f, 0.0f };

	// ループ用タイマー
	float loopTimer_ = 0.0f;

	// このフレームに放出予定の累積数
	uint32_t pendingBurstCount_ = 0;
};

