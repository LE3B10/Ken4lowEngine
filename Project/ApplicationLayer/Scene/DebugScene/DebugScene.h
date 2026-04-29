#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Derived/GuardianBoss/GuardianBoss.h"

#include <memory>
#include <array>
#include <cstdint>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;

	// 仮想3D描画処理
	void Draw3DObjects() override;

	// 仮想シャドウマップ描画処理
	void DrawShadowObjects() override;

	// 仮想2D描画処理
	void Draw2DSprites() override;

	// 仮想終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// デバッグカメラの更新
	void UpdateDebug();

	/// -------------------------------------------------------------
	/// 仮ヒット確認
	/// Hキーなどでボスに対して簡易球判定を飛ばす
	/// -------------------------------------------------------------
	void UpdateDebugBossHitTest();

	/// テスト用GPUパーティクル発火
	void UpdateDebugParticleTest();

	struct DebugParticlePreset
	{
		const char* label = "";
		K4E::GpuParticleType type = K4E::GpuParticleType::Default;
		float fadeInRatio = 0.1f;
		float fadeOutRatio = 0.25f;
		float emissiveBoost = 0.0f;
		float convergence = 0.0f;
		float divergence = 0.0f;
		float floaty = 0.0f;
		uint32_t spawnShapeOverride = 0;
		uint32_t burstCount = 32;
	};

	void EnsureDebugParticleEmitter();
	void ApplyDebugParticlePreset(uint32_t presetIndex);
	void TriggerDebugParticleBurst();
	void UpdateDebugParticleEmitterParams();

	/// -------------------------------------------------------------
	/// BossHitPart を文字列へ変換
	/// ログ確認用
	/// -------------------------------------------------------------
	const char* ToString(BossHitPart part) const;

private: /// ---------- メンバ変数 ---------- ///

	K4E::DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのポインタ
	K4E::Input* input_ = nullptr; // Inputのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突管理マネージャー

	// 描画確認用ボス
	std::unique_ptr<GuardianBoss> debugBoss_;

	// --- 仮ヒット確認用パラメータ ---
	bool debugBossHitTestEnabled_ = true; // 仮ヒット確認ON/OFF
	float debugHitRadius_ = 0.75f;        // 攻撃球の半径
	float debugBaseDamage_ = 10.0f;       // 基礎ダメージ

	std::string debugHitLog_ = "Press H to test hit.";

	// --- GPUパーティクルテスト用 ---
	std::string debugParticleLog_ = "DebugScene GPU particle lab ready.";

	std::string debugParticleEmitterName_ = "DebugScene_SpawnPrototype";
	Vector3 debugParticleSpawnPosition_{ 0.0f, 0.25f, 24.0f };
	int debugParticleBurstCount_ = 64;
	bool debugParticleAutoLoop_ = false;
	float debugParticleAutoInterval_ = 0.75f;
	float debugParticleAutoTimer_ = 0.0f;
	uint32_t debugParticlePresetIndex_ = 0;
	bool debugParticleDirty_ = true;

	float debugFadeInRatio_ = 0.08f;
	float debugFadeOutRatio_ = 0.20f;
	float debugEmissiveBoost_ = 1.5f;
	float debugConvergence_ = 0.0f;
	float debugDivergence_ = 0.0f;
	float debugFloaty_ = 0.0f;
	uint32_t debugSpawnShapeOverride_ = 0;

	static constexpr std::array<DebugParticlePreset, 4> kDebugParticlePresets_ = {{
		{ "Ground Telegragh", K4E::GpuParticleType::Dust, 0.08f, 0.35f, 0.4f, 0.0f, 0.7f, 0.15f, 4u, 56u },
		{ "Convergence", K4E::GpuParticleType::Ambient, 0.04f, 0.28f, 1.2f, 1.0f, 0.0f, 0.10f, 1u, 72u },
		{ "Materialize Assist", K4E::GpuParticleType::Smoke, 0.10f, 0.32f, 0.7f, 0.45f, 0.15f, 0.55f, 3u, 64u },
		{ "Completion Flash", K4E::GpuParticleType::Spark, 0.01f, 0.14f, 2.7f, 0.0f, 1.0f, 0.02f, 0u, 96u },
	}};
};

