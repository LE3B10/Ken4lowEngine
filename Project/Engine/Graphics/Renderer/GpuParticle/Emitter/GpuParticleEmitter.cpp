#include "GpuParticleEmitter.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　　　	コンストラクタ
/// -------------------------------------------------------------
GpuParticleEmitter::GpuParticleEmitter(const std::string& name, const EmitterInfo& info)
	: name_(name), info_(info)
{
}

/// -------------------------------------------------------------
///				　　　	射出要求
/// -------------------------------------------------------------
void GpuParticleEmitter::RequestEmit(uint32_t count)
{
	pendingBurstCount_ += count;
}

/// -------------------------------------------------------------
///				　　	定期発射の更新
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
	out.fadeInRatio = info_.fadeInRatio;
	out.fadeOutRatio = info_.fadeOutRatio;
	out.emissiveBoost = info_.emissiveBoost;
	out.convergence = info_.convergence;
	out.divergence = info_.divergence;
	out.floaty = info_.floaty;
	out.spawnShapeOverride = info_.spawnShapeOverride;

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

	// 消費（このフレーム分を確定）
	pendingBurstCount_ = 0;

	return true;
}


} // namespace Ken4lowEngine
