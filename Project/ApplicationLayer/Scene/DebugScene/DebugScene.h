#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "Enemy.h"
#include "MeleeEnemy.h"
#include "MidRangeEnemy.h"
#include "Player.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "DisintegrationDebugController.h"
#include "ApplicationLayer/DebugTools/FrustumCulling/FrustumCullingDebugController.h"
#include "ApplicationLayer/DebugTools/EnemyScalability/EnemyScalabilitySystem.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Stage.h"
#include "DataAssetPresets.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }
namespace Ken4lowEngine { class SkyBox; }
namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	~DebugScene() override;

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

	// 近接敵同士のXZ分離をまとめて解決する
	void ResolveMeleeEnemySeparation(float deltaTime);

	// SkyBox 設定はファイル欠落時だけ既定値へフォールバックする。
	void InitializeSkyBox();
	void ApplyActiveSkyBoxPreset(bool reloadTexture = false);
	bool LoadSkyBoxPresets();
	bool SaveSkyBoxPresets();
	void DrawSkyBoxImGui();
	void DrawEnemyStressTestImGui();

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
	std::unique_ptr<MeleeEnemy> debugMeleeEnemy_;
	std::unique_ptr<MidRangeEnemy> debugMidRangeEnemy_;

	// ステージ確認用（見た目＋衝突）
	std::unique_ptr<K4E::Stage> stage_;

	std::unique_ptr<K4E::SkyBox> skyBox_;
	K4E::SkyBoxPresetCollection skyBoxPresets_{};
	std::array<char, 256> skyBoxTexturePathBuffer_{};
	std::array<char, 256> cloudTexturePathBuffer_{};
	std::string skyBoxPresetLog_ = "SkyBox設定は未読み込みです。";

	// --- 仮ヒット確認用パラメータ ---
	bool debugBossHitTestEnabled_ = true; // 仮ヒット確認ON/OFF
	float debugHitRadius_ = 0.75f;        // 攻撃球の半径
	float debugBaseDamage_ = 10.0f;       // 基礎ダメージ

	std::string debugHitLog_ = "Press H to test hit.";

	// --- GPUパーティクルテスト用 ---
	std::string debugParticleLog_ = "Press 1/2/3 to test GPU particles.";

	std::unique_ptr<DisintegrationDebugController> disintegrationDebug_;

	std::unique_ptr<FrustumCullingDebugController> frustumCullingDebug_;

	// 通常 Enemy と混在させない StressTest 専用の軽量敵管理。
	K4E::EnemyScalabilitySystem enemyScalabilitySystem_{};
	int stressTestMeleeEnemyCount_ = 100;
	int stressTestMidRangeEnemyCount_ = 100;
	bool stageBoundsDebugDraw_ = true;
	bool frustumCullingDebugDraw_ = true;
	bool enemyShadowEnabled_ = true;

	K4E::Collider meleeDummyTarget_{};
	bool meleeDummyWireVisible_ = true;
	float meleeDummyWireRadius_ = 0.5f;

};
