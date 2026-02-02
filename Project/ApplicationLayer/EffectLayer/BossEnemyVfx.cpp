#include "BossEnemyVfx.h"
#include <algorithm>

/// -------------------------------------------------------------
///					ボス登場砂埃エフェクト更新
/// -------------------------------------------------------------
void BossEnemyVfx::UpdateAppearDust(const Vector3& position, uint32_t count)
{
	// エミッターが未取得なら取得を試みる
	if (!appearDustEmitter_)
	{
		// 念のため（Initialize順などの保険）
		appearDustEmitter_ = FindEmitter("BossAppear");
		if (!appearDustEmitter_) return;
	}

	SetPositionAndEmit(appearDustEmitter_, position, count);
}

void BossEnemyVfx::UpdateAura(const Vector3& position, uint32_t count)
{
	// エミッターが未取得なら取得を試みる
	if (!auraEmitter_)
	{
		// 念のため（Initialize順などの保険）
		auraEmitter_ = FindEmitter("BossAura");
		if (!auraEmitter_) return;
	}
	SetPositionAndEmit(auraEmitter_, position, count);
}

void BossEnemyVfx::UpdateRushTrail(float deltaTime, const Vector3& currentPosition, const Vector3& oldPosition, bool isRush)
{
	if (!isRush) return;

	if (!rushTrailEmitter_)
	{
		rushTrailEmitter_ = FindEmitter("BossRushTrail");
		if (!rushTrailEmitter_) return;
	}

	// 位置更新
	rushTrailEmitter_->SetPosition(currentPosition);

	// 速度に応じて発生数を増減（XZだけ見ると安定）
	Vector3 d = currentPosition - oldPosition;
	float distXZ = std::sqrt(d.x * d.x + d.z * d.z);
	float dt = (deltaTime > 0.00001f) ? deltaTime : 0.00001f;
	float speed = distXZ / dt;

	// 発生数計算（速度に応じて6～30の範囲で変化）
	uint32_t emitCount = static_cast<uint32_t>(std::clamp(speed, 15.0f, 50.0f));
	rushTrailEmitter_->RequestEmit(emitCount);
}

void BossEnemyVfx::UpdateRushHit(const Vector3& hitPosition, const Vector3& bossCenter)
{
	if (!rushHitEmitter_)
	{
		rushHitEmitter_ = FindEmitter("BossRushHit");
		if (!rushHitEmitter_) return;
	}
	if (rushHitCooldown_ > 0.0f) return;

	Vector3 pos = hitPosition;
	pos.y = bossCenter.y - 2.0f + 0.05f; // BossEnemy側の計算を移植

	rushHitEmitter_->SetPosition(pos);
	rushHitEmitter_->RequestEmit(64);

	rushHitCooldown_ = 0.20f;
}

void BossEnemyVfx::UpdateSpinAttack(const Vector3& currentPosition, bool isSpin)
{
	if (!isSpin) return;

	if (!spinAttackEmitter_)
	{
		spinAttackEmitter_ = FindEmitter("BossSpinAttack");
		if (!spinAttackEmitter_) return;
	}

	Vector3 pos = currentPosition;
	pos.y += 0.0f;
	SetPositionAndEmit(spinAttackEmitter_, pos, 10);
}

static Vector3 MakeShockwavePosition(const Vector3& center)
{
	Vector3 ringPos = center;
	ringPos.y -= 1.8f;
	return ringPos;
}

void BossEnemyVfx::UpdateDeathEffect(const Vector3& center, float deathTimer, bool& startBurstDone)
{
	// 死亡開始の一回だけ：爆発＋衝撃波＋破片埃
	if (!startBurstDone)
	{
		if (deathExplosionEmitter_) SetPositionAndEmit(deathExplosionEmitter_, center, 12);
		if (deathShockwaveEmitter_) SetPositionAndEmit(deathShockwaveEmitter_, MakeShockwavePosition(center), 6);
		if (debrisDustEmitter_) SetPositionAndEmit(debrisDustEmitter_, center, 48);

		startBurstDone = true;
	}

	// 死亡中しばらく：魂がふわっと上に昇る
	if (deathSoulEmitter_ && deathTimer < 1.6f)
	{
		Vector3 c = center;
		c.y += 1.2f;
		SetPositionAndEmit(deathSoulEmitter_, c, 2);
	}
}

/// -------------------------------------------------------------
///				　		エミッター登録
/// -------------------------------------------------------------
void BossEnemyVfx::RegisterEmitters()
{
	// もし再初期化される可能性があるなら、先にリセット
	Reset();

	// 共通：いったん白テクスチャ前提（存在するテクスチャ名に合わせて変更OK）
	const char* kDefaultTex = "white.png";

	// --- ボス登場砂埃 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Boss_Appear_Dust;
		info.radius = 3.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		appearDustEmitter_ = GetOrCreateEmitter("BossAppear", info);
	}

	// --- ボスオーラ ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Boss_Aura;
		info.radius = 0.5f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		auraEmitter_ = GetOrCreateEmitter("BossAura", info);
	}

	// --- ラッシュ軌跡 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Boss_Rush_Trail;
		info.radius = 0.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		rushTrailEmitter_ = GetOrCreateEmitter("BossRushTrail", info);
	}

	// --- ラッシュヒット（衝撃波で代用）---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Shockwave;
		info.radius = 0.5f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		rushHitEmitter_ = GetOrCreateEmitter("BossRushHit", info);
	}

	// --- スピン攻撃 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Boss_Spin_Slash;
		info.radius = 0.1f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		spinAttackEmitter_ = GetOrCreateEmitter("BossSpinAttack", info);
	}

	// --- 死亡：中心爆発 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Explosion_Fire;
		info.radius = 0.5f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		deathExplosionEmitter_ = GetOrCreateEmitter("BossDeathExplosion", info);
	}

	// --- 死亡：衝撃波 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Shockwave;
		info.radius = 0.15f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		deathShockwaveEmitter_ = GetOrCreateEmitter("BossDeathShockwave", info);
	}

	// --- 死亡：魂 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Boss_Death_Soul;
		info.radius = 0.35f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		deathSoulEmitter_ = GetOrCreateEmitter("BossDeathSoul", info);
	}

	// --- 死亡：破片埃 ---
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Boss_Debris_Dust;
		info.radius = 1.5f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardFlags = BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;

		debrisDustEmitter_ = GetOrCreateEmitter("BossDebrisDust", info);
	}
}
