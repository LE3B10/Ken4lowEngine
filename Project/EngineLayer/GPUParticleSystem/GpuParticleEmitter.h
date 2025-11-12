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
		GpuParticleType type = GpuParticleType::Default; // パーティクルの種類
		float radius = 0.0f;          // 発生範囲
		uint32_t loopCount = 0;       // ループ発生時に1回で出す数
		float loopFrequency = 0.0f;   // ループ発生周期(秒)。0ならループしない

		BillboardMode billboardMode = BillboardMode::Camera;
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	GpuParticleEmitter(const std::string& name, const EmitterInfo& info);

	// 射出要求
	void RequestEmit(uint32_t count);

	// 定期発射の更新
	bool BuildCB(GpuEmitterCBData& out, float deltaTime);

public: /// ---------- セッター ---------- ///

	// 座標を設定
	void SetPosition(const Vector3& position) { position_ = position; }

public: /// ---------- ゲッター ---------- ///

	const std::string& GetName() const { return name_; }

private: /// ---------- メンバ変数 ---------- ///

	std::string name_; // エミッター名
	EmitterInfo info_; // エミッター情報

	Vector3 position_{ 0.0f, 0.0f, 0.0f };

	// ループ用タイマー
	float loopTimer_ = 0.0f;

	// このフレームに放出予定の累積数
	uint32_t pendingBurstCount_ = 0;
};

