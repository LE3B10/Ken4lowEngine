#include "BossHeavyPunchAttack.h"

#include "GpuParticleType.h"

BossHeavyPunchAttack::BossHeavyPunchAttack()
	: BossMeleePhaseAttackBase(
		{ 0.55f, 0.12f, 0.12f, 0.80f, 2.20f, 0.0f, 6.50f },
		{ 40.0f, { 6.0f, 2.0f, 3.0f, 90.0f, 0.65f, 0.30f }, { 36, 0.35f, 1.0f, 1.0f }, "GuardianHeavyPunchImpact", Ken4lowEngine::GpuParticleType::Debris })
{
}
