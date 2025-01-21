#include "ParticleEmitter.h"
#include <LogString.h>
#include <DirectXCommon.h>

void ParticleEmitter::Initialize(std::string name)
{
	name_ = name;

	emitter.count = 3;
	emitter.frequency = 0.5f; // 0.5秒ごとに発生
	emitter.frequencyTime = 0.0f; // 発生時刻用の時刻、0で初期化

	emitter.transform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};
}

void ParticleEmitter::Update()
{
	particleGroups = ParticleManager::GetInstance()->GetParticleGroups();

	emitter.frequencyTime += kDeltaTime;

	if (emitter.frequency <= emitter.frequencyTime) {

		emitter.frequencyTime -= emitter.frequency;

		for (std::unordered_map<std::string, ParticleManager::ParticleGroup>::iterator particleGroupIterator = particleGroups.begin(); particleGroupIterator != particleGroups.end();) {

			ParticleManager::ParticleGroup* particleGroup = &(particleGroupIterator->second);

			ParticleManager::GetInstance()->Emit(name_, emitter.transform.translate, emitter.count);

			++particleGroupIterator;
		}
	}
}

void ParticleEmitter::Emit()
{
	ParticleManager::GetInstance()->Emit(name_, emitter.transform.translate, emitter.count);
}