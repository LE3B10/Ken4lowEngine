#pragma once
#include <cstdint>

/// -------------------------------------------------------------
///				GPUパーティクルの種類を管理する列挙型
/// -------------------------------------------------------------
enum class GpuParticleType : uint32_t
{
	Default = 0,         // デフォルト
	MuzzleFlash = 1,     // 銃口火花
	BulletTracer = 2,    // 弾道
	HitSpark = 3,        // 命中火花
	Blood = 4,           // 血しぶき
	Impact_Dust = 5,     // 地面衝撃
	Impact_Metal = 6,    // 金属衝撃
	Impact_Wood = 7,     // 木材衝撃
	Explosion_Fire = 8,  // 爆発火炎
	Explosion_Smoke = 9, // 爆発煙
	Foot_Dust = 10,      // 足元砂埃
	Env_Dust = 11,       // 環境砂埃
	Pickup_Glow = 12,    // アイテム取得光
	Skill_Effect = 13,   // スキルエフェクト
};