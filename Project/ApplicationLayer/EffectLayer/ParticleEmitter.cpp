#include "ParticleEmitter.h"
#include <LogString.h>
#include <DirectXCommon.h>
#include <ParticleManager.h>

ParticleEmitter::ParticleEmitter(ParticleManager* manager, const std::string& groupName)
	: particleManager_(manager), groupName_(groupName), position_({ 0.0f,0.0f,0.0f }),
	emissionRate_(10.0f), accumulatedTime_(0.0f)
{
}

void ParticleEmitter::Update()
{
    accumulatedTime_ += DirectXCommon::GetInstance()->GetFPSCounter().GetFPS();

    // 発生させるパーティクルの数を計算
    int particleCount = static_cast<int>(emissionRate_);
    if (particleCount > 0) {
        particleManager_->Emit(groupName_, position_, particleCount, ParticleEffectType::Slash);
        accumulatedTime_ -= static_cast<float>(particleCount) / emissionRate_;
    }
}
