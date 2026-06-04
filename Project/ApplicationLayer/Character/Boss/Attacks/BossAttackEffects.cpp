#define NOMINMAX
#include "BossAttackEffects.h"
#include "GpuParticleType.h"
#include "GpuParticleManager.h"
#include "GpuParticleEmitter.h"

#include <algorithm>

void BossAttackEffects::EmitGuardianHitEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	if (spawnCount == 0 || emitterName == nullptr) return;

	if (auto* particleManager = Ken4lowEngine::GpuParticleManager::GetInstance())
	{
		// ボス攻撃共通の命中GPUパーティクル生成を1箇所に集約し、各攻撃の命中演出を揃える。
		Ken4lowEngine::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "Effects/white.dds";
		info.radius = std::max(0.0f, spawnRadius);
		info.kind = Ken4lowEngine::GpuParticleKind::Sprite;
		info.spriteType = particleType;
		info.billboardFlags = Ken4lowEngine::BillboardMode::Camera;
		info.lifeScale = std::max(0.01f, lifetimeScale);
		info.speedScale = std::max(0.0f, initialSpeedScale);

		if (auto* emitter = particleManager->GetEmitter(emitterName))
		{
			emitter->GetInfoMutable() = info;
			emitter->SetPosition(position);
			emitter->RequestEmit(spawnCount);
		}
		else if (auto* created = particleManager->CreateEmitter(emitterName, info))
		{
			created->SetPosition(position);
			created->RequestEmit(spawnCount);
		}
	}
}
