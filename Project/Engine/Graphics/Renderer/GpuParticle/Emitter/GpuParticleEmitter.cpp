#include "GpuParticleEmitter.h"

#include <algorithm>

namespace Ken4lowEngine
{
namespace
{
	float EstimateGpuParticleLifeTimeSec(GpuParticleType type)
	{
		switch (type)
		{
		case GpuParticleType::Blood: return 0.80f;
		case GpuParticleType::Dust: return 1.20f;
		case GpuParticleType::Debris: return 1.00f;
		case GpuParticleType::Smoke: return 3.00f;
		case GpuParticleType::Ambient: return 7.00f;
		case GpuParticleType::Spark: return 0.22f;
		case GpuParticleType::Shockwave: return 0.50f;
		case GpuParticleType::Heal: return 1.40f;
		case GpuParticleType::Trail: return 0.22f;
		case GpuParticleType::DeathBurstCore: return 0.28f;
		case GpuParticleType::PlayerDamageBlood: return 0.55f;
		case GpuParticleType::MuzzleFlash: return 0.085f;
		case GpuParticleType::BulletTracer: return 0.30f;
		case GpuParticleType::Default:
		default:
			return 1.00f;
		}
	}
}

/// -------------------------------------------------------------
///					　　　コンストラクタ
/// -------------------------------------------------------------
GpuParticleEmitter::GpuParticleEmitter(const std::string& name, const EmitterInfo& info)
	: name_(name), info_(info)
{
}

/// -------------------------------------------------------------
///					　　　射出要求
/// -------------------------------------------------------------
void GpuParticleEmitter::RequestEmit(uint32_t count)
{
	pendingBurstCount_ += count;
}

void GpuParticleEmitter::UpdateActivity(float deltaTime)
{
	for (auto& batch : activeBatches_)
	{
		batch.remainingSec -= deltaTime;
	}

	while (!activeBatches_.empty() && activeBatches_.front().remainingSec <= 0.0f)
	{
		estimatedActiveParticleCount_ -= std::min(estimatedActiveParticleCount_, activeBatches_.front().count);
		activeBatches_.pop_front();
	}
}

float GpuParticleEmitter::EstimateParticleLifeTimeSec() const
{
	// HLSL の lifeMax と合わせ、寿命後は Draw 対象から外して常時フル描画を避ける。
	return EstimateGpuParticleLifeTimeSec(static_cast<GpuParticleType>(GetEffectiveType())) + 0.10f;
}

void GpuParticleEmitter::RegisterActiveBatch(uint32_t count)
{
	if (count == 0)
	{
		return;
	}

	activeBatches_.push_back({ EstimateParticleLifeTimeSec(), count });
	estimatedActiveParticleCount_ += count;
}

/// -------------------------------------------------------------
///				　　定期発射の更新
/// -------------------------------------------------------------
bool GpuParticleEmitter::BuildCB(GpuEmitterCBData& out, float deltaTime)
{
	// ------------------------------
	// ループ発生（定期発生）
	// ------------------------------
	if (info_.loopFrequency > 0.0f && info_.loopCount > 0)
	{
		loopTimer_ += deltaTime;

		// 周期を超えた分だけ発生（取りこぼし防止でwhile）
		while (loopTimer_ >= info_.loopFrequency)
		{
			pendingBurstCount_ += info_.loopCount;
			loopTimer_ -= info_.loopFrequency;
		}
	}

	// ------------------------------
	// 共通でCBに書く値（Emitしない場合も）
	// ------------------------------
	out.translate = position_;
	out.radius = info_.radius;
	out.frequency = info_.loopFrequency;
	out.frequencyTime = loopTimer_; // デバッグ用に残す（不要なら0でもOK）

	// kindに応じた type / packed billboardMode
	out.type = GetEffectiveType();
	out.billboardMode = GetPackedBillboardMode();

	// ------------------------------
	// Emitしない
	// ------------------------------
	if (pendingBurstCount_ == 0)
	{
		out.emit = 0;
		out.count = 0;
		return false;
	}

	// ------------------------------
	// Emitする
	// ------------------------------
	out.emit = 1;
	out.count = pendingBurstCount_;
	out.frequencyTime = 0.0f; // 使わないなら0固定でOK

	// 生存数をCPU側でも概算追跡し、寿命終了後の不要な全エミッター描画を止める。
	RegisterActiveBatch(pendingBurstCount_);

	// 消費（このフレーム分を確定）
	pendingBurstCount_ = 0;

	return true;
}


} // namespace Ken4lowEngine
