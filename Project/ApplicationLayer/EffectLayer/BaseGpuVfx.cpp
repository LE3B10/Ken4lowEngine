#include "BaseGpuVfx.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BaseGpuVfx::Initialize(GpuParticleManager* manager, const std::string& prefix)
{
	manager_ = manager;
	prefix_ = prefix;
	RegisterEmitters();
}

/// -------------------------------------------------------------
///							名前生成
/// -------------------------------------------------------------
std::string BaseGpuVfx::MakeName(const std::string& key)
{
	if (prefix_.empty()) { return key; }
	return prefix_ + "_" + key;
}

/// -------------------------------------------------------------
///						エミッター取得・作成
/// -------------------------------------------------------------
GpuParticleEmitter* BaseGpuVfx::GetOrCreateEmitter(const std::string& key, const GpuParticleEmitter::EmitterInfo& info)
{
	if (!manager_) { return nullptr; }

	const std::string name = MakeName(key);

	// CreateEmitter は同名があると nullptr を返すので、先に Get する
	if (auto* emitter = manager_->GetEmitter(name))
	{
		return emitter;
	}
	return manager_->CreateEmitter(name, info);
}

/// -------------------------------------------------------------
///						 エミッター検索
/// -------------------------------------------------------------
GpuParticleEmitter* BaseGpuVfx::FindEmitter(const std::string& key)
{
	if (!manager_) { return nullptr; }
	return manager_->GetEmitter(MakeName(key));
}

/// -------------------------------------------------------------
///					位置設定とパーティクル放出
/// -------------------------------------------------------------
void BaseGpuVfx::SetPositionAndEmit(GpuParticleEmitter* emitter, const Vector3& position, uint32_t count)
{
	if (!emitter) { return; }
	emitter->SetPosition(position);
	emitter->RequestEmit(count);
}
