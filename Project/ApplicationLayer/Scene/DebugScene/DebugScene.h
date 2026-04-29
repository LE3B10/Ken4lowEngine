#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Derived/GuardianBoss/GuardianBoss.h"

#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }

class DebugScene : public BaseScene
{
public:
	void Initialize() override;
	void Update() override;
	void Draw3DObjects() override;
	void DrawShadowObjects() override;
	void Draw2DSprites() override;
	void Finalize() override;
	void DrawImGui() override;

private:
	void UpdateDebug();
	void UpdateDebugBossHitTest();
	void UpdateDebugParticleTest();
	void EnsureDebugParticleEmitter();
	void TriggerDebugParticleBurst();
	const char* ToString(BossHitPart part) const;

private:
	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;
	bool isDebugCamera_ = false;

	std::unique_ptr<CollisionManager> collisionManager_;
	std::unique_ptr<GuardianBoss> debugBoss_;

	bool debugBossHitTestEnabled_ = true;
	float debugHitRadius_ = 0.75f;
	float debugBaseDamage_ = 10.0f;
	std::string debugHitLog_ = "Press H to test hit.";

	// --- GPUパーティクル最小テスト用 ---
	std::string debugParticleEmitterName_ = "DebugScene_MinimalSpark";
	Vector3 debugParticleSpawnPosition_{ 0.0f, 0.25f, 24.0f };
	int debugParticleBurstCount_ = 32;
	std::string debugParticleLog_ = "Press P or click Burst.";
};
