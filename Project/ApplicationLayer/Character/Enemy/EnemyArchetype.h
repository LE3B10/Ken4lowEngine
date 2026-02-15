#pragma once
#include <cstdint>

/// --------------------------------------------------------------
///				　		敵のアーキタイプ（役割）
/// --------------------------------------------------------------
enum class EnemyArchetype : uint8_t
{
	RifleGrunt,		// ライフルを持った雑魚
	SMGFlanker,		// サブマシンガンを持ったフランカー
	Sniper,			// スナイパー
};

/// --------------------------------------------------------------
///				　	敵の行動パターン設定
/// --------------------------------------------------------------
struct EnemyTuning
{
	// 共通
	float moveSpeed = 3.0f;
	float attackRange = 15.0f;
	float viewRange = 25.0f;

	// 射撃
	float fireInterval = 0.35f;
	int burstMin = 2;
	int burstMax = 4;

	// GunAI（距離管理/ストレイフ）
	float preferredMinRatio = 0.35f; // attackRangeに対する割合
	float preferredMaxRatio = 0.65f;
	float strafeSpeedMul = 0.75f;
	float aimMoveMul = 0.60f;
	float burstMoveMul = 0.20f;

	// 命中ブレ（度）
	float spreadNearDeg = 0.5f;
	float spreadFarDeg = 2.8f;

	// 反応
	float reactionDelaySec = 0.25f;

	// 弾（Enemy::FireAt が使う値に合わせる）
	float bulletSpeed = 80.0f;
	float bulletLifeSec = 2.0f;
	int   bulletDamage = 1;
};