#pragma once
#include <cstdint>

enum class GpuParticleKind : uint32_t
{
	Sprite = 0,
	Mesh = 1,
	Ribbon = 2,
	Beam = 3,
};

// --- pack（HLSLと一致）---
static constexpr uint32_t GPU_PARTICLE_KIND_SHIFT = 16;
static constexpr uint32_t GPU_PARTICLE_KIND_MASK = 0x00FF0000u;
static constexpr uint32_t GPU_PARTICLE_BB_MASK = 0x0000FFFFu;

static constexpr uint32_t PackBillboardMode(GpuParticleKind kind, uint32_t bbFlags)
{
	return ((static_cast<uint32_t>(kind) << GPU_PARTICLE_KIND_SHIFT) & GPU_PARTICLE_KIND_MASK) |
		(bbFlags & GPU_PARTICLE_BB_MASK);
}

/// -------------------------------------------------------------
///				GPUパーティクルの種類を管理する列挙型
/// -------------------------------------------------------------
enum class GpuParticleType : uint32_t
{
	Default = 0,			// デフォルト
	MuzzleFlash = 1,		// 銃口火花
	BulletTracer = 2,		// 弾道
	HitSpark = 3,			// 命中火花
	Blood = 4,				// 血しぶき
	Impact_Dust = 5,		// 地面衝撃
	Impact_Metal = 6,		// 金属衝撃
	Impact_Wood = 7,		// 木材衝撃
	Explosion_Fire = 8,		// 爆発火炎
	Explosion_Smoke = 9,	// 爆発煙
	Foot_Dust = 10,			// 足元砂埃
	Env_Dust = 11,			// 環境砂埃
	Pickup_Glow = 12,		// アイテム取得光
	Skill_Effect = 13,		// スキルエフェクト
	Boss_Appear_Dust = 14,  // ボス登場砂埃
	Boss_Aura = 15,         // ボスオーラ
	Boss_Rush_Trail = 16,   // ボスラッシュトレイル
	Shockwave = 17,			// 衝撃波
	Boss_Spin_Slash = 18,   // 回転斬り
	Boss_Death_Soul = 19,	// ボス死亡魂エフェクト
	Boss_Debris_Dust = 20,	// ボス破片埃
	Heal_Effect = 21,       // 回復エフェクト
};

enum class GpuRibbonType : uint32_t
{
	BulletTracer = 0,
	BossRushTrail = 1,
	BossSpinSlash = 2,
};

// RibbonType → 既存の SpawnByType 用 type にマップ（当面はこれでOK）
static constexpr GpuParticleType ToGpuParticleType(GpuRibbonType t)
{
	switch (t)
	{
	case GpuRibbonType::BulletTracer:  return GpuParticleType::BulletTracer;
	case GpuRibbonType::BossRushTrail: return GpuParticleType::Boss_Rush_Trail;
	case GpuRibbonType::BossSpinSlash: return GpuParticleType::Boss_Spin_Slash;
	default:                           return GpuParticleType::BulletTracer;
	}
}