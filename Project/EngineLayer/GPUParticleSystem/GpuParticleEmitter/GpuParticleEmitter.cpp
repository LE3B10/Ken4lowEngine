#include "GpuParticleEmitter.h"

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
	// ループ発生(デフォルトパーティクル用)
	if (info_.loopFrequency > 0.0f && info_.loopCount > 0)
	{
		loopTimer_ += deltaTime; // タイマー加算

		// 発生周期を超えたら発生数を加算
		while (loopTimer_ >= info_.loopFrequency)
		{
			pendingBurstCount_ += info_.loopCount;
			loopTimer_ -= info_.loopFrequency;
		}
	}

	// 発生数が0なら何もしない
	if (pendingBurstCount_ == 0)
	{
		out.emit = 0;
		out.count = 0;
		out.translate = position_;
		out.radius = info_.radius;
		out.frequency = info_.loopFrequency;
		out.frequencyTime = loopTimer_; // 使わないなら 0.0f でもOK
		out.type = static_cast<uint32_t>(info_.type);
		out.billboardMode = static_cast<uint32_t>(info_.billboardMode);
		return false;
	}

	// CBに書き込み
	out.translate = position_;
	out.radius = info_.radius;
	out.count = pendingBurstCount_;
	out.frequency = info_.loopFrequency;
	out.frequencyTime = 0.0f; // 使わないなら0でOK
	out.emit = 1;
	out.type = static_cast<uint32_t>(info_.type);
	out.billboardMode = static_cast<uint32_t>(info_.billboardMode);

	// 消費
	pendingBurstCount_ = 0;

	return true;
}

