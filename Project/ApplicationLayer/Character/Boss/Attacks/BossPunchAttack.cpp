#include "BossPunchAttack.h"

#include "GpuParticleType.h"

BossPunchAttack::BossPunchAttack()
	: BossMeleePhaseAttackBase(
		{ 0.30f, 0.0f, 0.12f, 0.40f, 1.10f, 0.0f, 5.75f },
		{ 20.0f, { 6.0f, 2.0f, 3.0f, 90.0f, 0.65f, 0.25f }, { 36, 0.35f, 1.0f, 1.0f }, "GuardianPunchImpact", Ken4lowEngine::GpuParticleType::Spark })
{
}
