#pragma once
#include "CollisionManager.h"
#include "BulletManager.h"
#include "CharacterWorld.h"
#include "HUDManager.h"
#include "WaveManager.h"
#include "Stage.h"
#include "StageObjectiveManager.h"
#include "GamePlayStageContext.h"
#include <SkyBox.h>
#include "DataAssetPresets.h"
#include "EnemyHPBarManager.h"
#include "ItemManager.h"
#include "CrystalManager.h"
#include "BossBattleController.h"
#include "AimTargetDetector.h"
#include "Stage1TutorialController.h"
#include "GameplayPhysicsDebugController.h"
#include "AmmoRecoveryItemSpawner.h"
#include "WorldDebugView.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

class Player;
/// -------------------------------------------------------------
/// GamePlayScene内のランタイムWorld管理クラス
///
/// ステージ、キャラクター、弾、HUD、Wave、クリスタル、ボス、アイテム、衝突を所有し、
/// GamePlaySceneから1フレームごとに更新・描画される。
/// GamePlaySceneより短い寿命で、リトライ時には丸ごと再生成される前提。
/// -------------------------------------------------------------
class GamePlayWorld
{
private:

public: /// ---------- メンバ関数 ---------- ///

	// StageContextのステージ情報をもとに、ステージ/キャラクター/衝突/HUD/目的管理を生成する。
	void Initialize(GamePlayStageContext& stageContext, bool skipStage1Tutorial = false);
	// CollisionManager登録を外し、所有するWorld内オブジェクトとグローバル参照を解放する。
	void Finalize();

	// World内のステージ、キャラクター、ボス、弾、当たり判定、HUD、目的条件を1フレーム進める。
	void Update(float deltaTime);

	// イントロ中に表示に必要なステージ・影・空だけを更新する。
	void UpdateIntroVisuals();
	// 武器構えイントロ中に、通常AIは止めたままプレイヤー表示だけを更新する。
	void UpdateEquipIntro(float deltaTime);
	// イントロ終了直後の初回フレーム負荷を避けるため、敵や武器表示を非表示で事前更新する。
	void WarmupStartGameplayForIntro();
	// イントロ用に非表示で温めたプレイヤー/武器/敵の見た目を切り替える。
	void SetStartGameplayVisualsVisible(bool visible);

	// 3D要素を描画する。イントロ中はキャラクターだけを隠して背景確認を優先できる。
	void Draw3D(bool hideCharactersDuringIntro);
	// ボス登場演出中に必要な空・ステージ・ボスだけを描画する。
	void DrawBossIntro3D();
	// シャドウ用描画を行う。通常描画と同じイントロ非表示条件を使う。
	void DrawShadow(bool hideCharactersDuringIntro);
	// ボス登場演出中に必要なステージ・ボスだけの影を描画する。
	void DrawBossIntroShadow();
	// HUDと敵HPバーを描画する。イントロ中は呼び出し側から隠せる。
	void DrawHUD(bool hideDuringIntro);
	void DrawImGui();
	void DrawGameDebugImGui();
	void DrawCollisionDebugImGui();
	void DrawEnemyDebugImGui();

	void SyncAfterPlayerSpawn();
	void StartWaves();

	bool IsPlayerDead();
	bool IsAllWavesCleared() const;

	bool IsStageObjectiveCleared() const;
	bool IsStageObjectiveFailed() const;
	bool IsGameClearRequested() const { return bossBattleController_.IsGameClearRequested(); }

	void SetDebugCameraEnabled(bool enabled);

	// 防衛対象の実オブジェクト破壊イベントが接続されるまで、デバッグ/仮実装から失敗条件を反映する窓口。
	void SetDefenseTargetDestroyed(bool destroyed);

	CharacterWorld& GetCharacters() { return characters_; }
	const CharacterWorld& GetCharacters() const { return characters_; }

	HUDManager* GetHUDManager() const { return hudManager_.get(); }
	// Details用の弾数参照はWorld経由で安全にnullptr確認してから使う。
	BulletManager* GetBulletManager() const { return bulletManager_.get(); }
	WaveManager* GetWaveManager() const { return waveManager_.get(); }
	K4E::Stage* GetStage() const { return stage_.get(); }
	K4E::SkyBox* GetSkyBox() const { return skyBox_.get(); }
	CollisionManager* GetCollisionManager() const { return collisionManager_.get(); }

	const K4E::Matrix4x4& GetShadowLightViewProjection() const
	{
		return shadowLightViewProjection_;
	}

	// 装置オブジェクトの接触イベント実装後は、各装置から呼ばれる想定の進行加算窓口。
	void AddActivatedDeviceCount(int amount = 1);
	// ゴール接触判定が実オブジェクト側へ移るまで、到達フラグを外部から反映する窓口。
	void SetReachedGoal(bool reached);
	// ボス撃破/クリアアイテム取得の結果をステージ目的管理へ反映する。
	void SetBossDefeated(bool defeated);
	// クリスタル破壊イベントから将来のボス登場演出へつなぐための読み取り口。
	bool HasCrystalBroken() const { return crystalManager_.HasCrystalBroken(); }
	bool IsFinalPhaseReady() const { return crystalManager_.IsFinalPhaseReady(); }
	bool IsBossAppearRequested() const { return crystalManager_.IsBossAppearRequested(); }
	bool IsWorldColorChangeComplete() const { return crystalManager_.IsWorldColorChangeComplete(); }
	bool IsBossIntroActive() const { return bossBattleController_.IsIntroActive(); }
	bool IsBossIntroGameplayPaused() const { return bossBattleController_.IsIntroGameplayPaused(); }
	// カメラ演出中は通常3D/HUDを止め、専用描画だけに切り替える。
	bool IsBossIntroPresentationActive() const { return bossBattleController_.IsIntroPresentationActive(); }
	bool HasStage1TutorialCompleted() const { return stage1TutorialController_.HasCompletedTutorial(); }

private: /// ---------- メンバ関数 ---------- ///

	// Initializeを責務ごとに分け、World生成時の副作用を追いやすくする。
	void InitializeLighting();
	void InitializeSkyBox();
	void InitializeCollisionSystems();
	void InitializeCharacterSystems();
	void InitializeHUD();
	void InitializeStageAndPhysics(const GamePlayStageContext::StageAssetPaths& stageAssets);
	void InitializePlayerSpawn(GamePlayStageContext& stageContext);
	void InitializeWaveSystem(GamePlayStageContext& stageContext);
	void InitializeBossState(GamePlayStageContext& stageContext);
	void InitializeStage1Crystals();
	void InitializeRuntimeHelpers();

	// Updateを責務ごとに分け、ステージ進行・戦闘・HUD更新の混在を抑える。
	void UpdateStageRuntime();
	bool UpdateBlockingStage1Intro(float deltaTime);
	bool UpdateBlockingBossIntro(float deltaTime);
	void UpdateGameplayActors(float deltaTime);
	void UpdatePlayerLadderOverlap();
	void UpdateBossRuntime(float deltaTime);
	void UpdateItemRuntime(float deltaTime);
	void UpdateShadowRuntime();
	void UpdateBulletAndCollisionRuntime(float deltaTime);
	void UpdateSkyBoxRuntime(float deltaTime);
	void UpdateAimTargetRuntime();
	void UpdateHpBarRuntime(float deltaTime);
	void UpdateHudRuntime(float deltaTime);
	void UpdateWaveRuntime(float deltaTime);
	void UpdateStageObjectiveRuntime(float deltaTime);

	void CollisionUpdate();
	GameplayPhysicsDebugController::Dependencies BuildGameplayPhysicsDebugDependencies();
	void UpdateShadowLightViewProjection();
	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const;
	bool IsSightBlocked(const K4E::Segment& seg) const;
	Stage1TutorialController::Dependencies BuildStage1TutorialDependencies();
	BossBattleController::Dependencies BuildBossBattleDependencies();
	WorldDebugView::Dependencies BuildWorldDebugDependencies();


private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<CollisionManager> collisionManager_;
	std::unique_ptr<BulletManager> bulletManager_;
	CharacterWorld characters_;

	std::unique_ptr<K4E::SkyBox> skyBox_ = nullptr;
	K4E::SkyBoxPresetCollection skyBoxPresets_{};
	std::unique_ptr<K4E::Stage> stage_ = nullptr;
	std::unique_ptr<WaveManager> waveManager_ = nullptr;
	std::unique_ptr<HUDManager> hudManager_ = nullptr;

	EnemyHPBarManager enemyHpBarManager_;
	ItemManager itemManager_;
	AmmoRecoveryItemSpawner ammoRecoveryItemSpawner_; // 弾切れ詰み防止用AmmoSmallの時間スポーンをWorld本体から分離する。
	CrystalManager crystalManager_;
	AimTargetDetector aimTargetDetector_;
	GameplayPhysicsDebugController gameplayPhysicsDebugController_{}; // Gameplay側PhysicsWorldのDebug/移行テストをWorld本体から分離する。
	WorldDebugView worldDebugView_{}; // GamePlayWorld周辺のDebug ImGui表示をWorld本体から分離する。

	int prevWaveNumber_ = 0;
	bool prevWaveInProgress_ = false;
	bool prevAllWavesCleared_ = false;

	K4E::Matrix4x4 shadowLightViewProjection_{};

	K4E::Vector3 shadowLightDirection_ = { 0.3f, -1.0f, 0.2f };
	float shadowDistance_ = 50.0f;
	float shadowOrthoHalfWidth_ = 25.0f;
	float shadowOrthoHalfHeight_ = 25.0f;
	float shadowNearZ_ = 0.1f;
	float shadowFarZ_ = 120.0f;

	std::unique_ptr<StageObjectiveManager> stageObjectiveManager_ = nullptr;

	float lastBulletUpdateMs_ = 0.0f;
	float lastCollisionUpdateMs_ = 0.0f;
	bool stage1BeginnerBalanceEnabled_ = false;
	bool skipStage1Tutorial_ = false;
	Stage1TutorialController stage1TutorialController_{}; // ステージ1専用チュートリアルの状態と進行をWorld本体から分離する。
	BossBattleController bossBattleController_{}; // ボス登場からクリアアイテム取得までの進行をWorld本体から分離する。
};
