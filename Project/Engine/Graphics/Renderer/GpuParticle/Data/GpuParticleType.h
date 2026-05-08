#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

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
		Default = 0,

		Blood,
		Dust,
		Debris,
		Smoke,
		Ambient,
		Spark,
		Shockwave,
		Heal,
		Trail,
		DeathBurstCore,

		// 相互評価で多かった「被弾」「射撃」「弾道」の手応えを出すための追加タイプ
		PlayerDamageBlood,
		MuzzleFlash,
		BulletTracer,

		// 被弾時に装甲ブロックが欠けて飛び散る表現用
		ArmorBreak,

		// Voxel Disintegrationで元オブジェクトから剥がれた小ブロック片を表現するタイプ
		VoxelFragment,

		// 指パッチン風に灰化しながら流れて消えるVoxel破片
		VoxelAshFragment,

		Count
	};

	enum class GpuRibbonType : uint32_t
	{
		Trail = 0,
	};

	// RibbonType → 既存の SpawnByType 用 type にマップ（当面はこれでOK）
	static constexpr GpuParticleType ToGpuParticleType(GpuRibbonType t)
	{
		switch (t)
		{
		case GpuRibbonType::Trail:  return GpuParticleType::Trail;
		default:                    return GpuParticleType::Trail;
		}
	}
} // namespace Ken4lowEngine
