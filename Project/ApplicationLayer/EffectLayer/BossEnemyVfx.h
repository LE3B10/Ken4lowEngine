#pragma once
#include "BaseGpuVfx.h"
#include "GpuParticleType.h"
#include "BillboardMode.h"

/// -------------------------------------------------------------
///          Boss専用VFX（まずは登場エフェクトだけ）
/// -------------------------------------------------------------
class BossEnemyVfx : public BaseGpuVfx
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~BossEnemyVfx() = default;

	// 砂埃エフェクト更新
	void UpdateAppearDust(const Vector3& position, uint32_t count);

	// ボスオーラエフェクト更新
	void UpdateAura(const Vector3& position, uint32_t count);

	// ラッシュ軌跡エフェクト更新
	void UpdateRushTrail(float deltaTime, const Vector3& currentPosition, const Vector3& oldPosition, bool isRush);

	// ラッシュヒットエフェクト更新
	void UpdateRushHit(const Vector3& hitPosition, const Vector3& bossCenter);

	// スピン攻撃エフェクト更新
	void UpdateSpinAttack(const Vector3& currentPosition, bool isSpin);

	// 死亡時エフェクト更新
	void UpdateDeathEffect(const Vector3& center, float deathTimer, bool& startBurstDone);

	// 連続発生防止のクールダウン更新
	void Tick(float deltaTime) { if (rushHitCooldown_ > 0.0f) rushHitCooldown_ -= deltaTime; }

protected: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 派生先でエミッターを登録する純粋仮想メソッド。
	/// </summary>
	void RegisterEmitters() override;

private: /// ---------- メンバ変数 ---------- ///

	GpuParticleEmitter* appearDustEmitter_ = nullptr;	  // 登場時の砂埃エミッター
	GpuParticleEmitter* auraEmitter_ = nullptr;			  // ボスオーラエミッター
	GpuParticleEmitter* rushTrailEmitter_ = nullptr;	  // ラッシュ軌跡用エミッター
	GpuParticleEmitter* rushHitEmitter_ = nullptr;		  // ラッシュヒット用エミッター
	GpuParticleEmitter* spinAttackEmitter_ = nullptr;	  // スピン攻撃用エミッター
	GpuParticleEmitter* deathExplosionEmitter_ = nullptr; // 中心爆発
	GpuParticleEmitter* deathShockwaveEmitter_ = nullptr; // 足元衝撃波リング
	GpuParticleEmitter* deathSoulEmitter_ = nullptr;      // 魂が昇る
	GpuParticleEmitter* debrisDustEmitter_ = nullptr;     // 破片の埃（余韻）

	float rushHitCooldown_ = 0.0f; // 連続発生防止(秒)
};

