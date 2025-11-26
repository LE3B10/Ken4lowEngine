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
	{ // ボス登場砂埃エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Boss_Appear_Dust; // ボス登場砂埃
		info.radius = 3.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";
		appearDustEmitter_ = GetOrCreateEmitter("BossAppear", info);
	}

	{ // ボスオーラエミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Boss_Aura;   // ボスオーラ
		info.radius = 0.5f;                       // ボス中心からの広がり
		info.loopCount = 0;	                       // ループは使わず毎フレーム RequestEmit 方式
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";
		auraEmitter_ = GetOrCreateEmitter("BossAura", info);
	}

	{ // ラッシュ軌跡エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Boss_Rush_Trail;
		info.radius = 0.0f;         // 細め（太いなら 0.2f へ）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;   // ループは使わず、Rush中に RequestEmit する
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";
		rushTrailEmitter_ = GetOrCreateEmitter("BossRushTrail", info);
	}

	{ // スピン攻撃エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Boss_Spin_Slash;
		info.radius = 0.1f;         // ボス中心からの広がり
		info.loopCount = 0;
		info.loopFrequency = 0.0f;   // ループは使わず、Spin中に RequestEmit する
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";
		spinAttackEmitter_ = GetOrCreateEmitter("BossSpinAttack", info);
	}

	{ // 死亡中心爆発エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Explosion_Fire; // 小さな爆発
		info.radius = 0.5f;                          // ボス中心周りにちょっと広がる
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";          // 炎っぽいテクスチャあるなら差し替え
		deathExplosionEmitter_ = GetOrCreateEmitter("BossDeathExplosion", info);
	}

	{ // 死亡衝撃波エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Shockwave;      // 衝撃波タイプ（GPU_PARTICLE_TYPE_SHOCKWAVE と一致）
		info.radius = 0.15f;                         // ほぼ中心から出す
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";
		deathShockwaveEmitter_ = GetOrCreateEmitter("BossDeathShockwave", info);
	}

	{ // 死亡魂エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Boss_Death_Soul;
		info.radius = 0.35f;                            // 発生の散り幅（小さめ）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;                      // UpdateDeathでRequestEmitする
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";           // ふわっと感が出やすい
		deathSoulEmitter_ = GetOrCreateEmitter("BossDeathSoul", info);
	}

	{ // 破片埃エミッター登録
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Boss_Debris_Dust;
		info.radius = 1.5f;                             // 破片周りに散る想定で少し広め
		info.loopCount = 0;
		info.loopFrequency = 0.0f;                      // 死亡開始の瞬間だけバースト
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";             // 粒埃はwhiteでOK
		debrisDustEmitter_ = GetOrCreateEmitter("BossDebrisDust", info);
	}

	{
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Shockwave;
		info.radius = 0.5f;                          // 発生の散り幅（小さめでOK）
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";         // 衝撃波っぽいならこれが相性良い
		rushHitEmitter_ = GetOrCreateEmitter("BossRushHit", info);
	}
}
