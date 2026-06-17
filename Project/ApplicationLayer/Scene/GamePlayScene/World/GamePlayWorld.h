#pragma once
#include "CollisionManager.h"
#include "BulletManager.h"
#include "CharacterWorld.h"
#include "HUDManager.h"
#include "WaveManager.h"
#include "Stage.h"
#include "StageObjectiveManager.h"
#include <SkyBox.h>
#include "DataAssetPresets.h"
#include "EnemyHPBarManager.h"
#include "ItemManager.h"
#include "CrystalManager.h"
#include "BossIntroController.h"
#include "AimTargetDetector.h"
#include "GameplayPhysicsEventHandler.h"
#include "PhysicsTestBullet.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "Object3D.h"
#include "CollisionSystemPolicy.h"
#include "StagePhysicsBinder.h"
#include "PhysicsParameterBridge.h"
#include "PhysicsWorld.h"
#include "PhysicsDebugDraw.h"
#include "Rigidbody.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

class Player;
/// -------------------------------------------------------------
/// ボス撃破後に出現するクリア用アイテム
///
/// GamePlayWorldが生成・所有し、CollisionManagerへ登録する。
/// 取得後は遠方へ移動して当たり判定から外し、World側のクリア判定へ通知する。
/// -------------------------------------------------------------
class BossClearItem : public K4E::Collider
{
public:
	// ボス位置を基準に表示モデルとItem判定を生成する。CollisionManager登録は呼び出し側が行う。
	void Initialize(const K4E::Vector3& position);
	// 浮遊・回転演出とCollider中心を同期する。
	void Update(float deltaTime);
	// 未取得かつ生成済みのときだけモデルを描画する。
	void Draw();
	// プレイヤー中心との距離で取得可能か判定する。実取得処理はGamePlayWorld側で行う。
	bool CheckPickup(const Player& player) const;
	// 取得済みにして判定を遠方へ逃がす。CollisionManagerからの削除は呼び出し側が行う。
	void MarkCollected();

	bool IsSpawned() const { return spawned_; }
	bool IsCollected() const { return collected_; }
	const K4E::Vector3& GetPosition() const { return position_; }

	void OnCollision(K4E::Collider* other) override;
	K4E::Vector3 GetCenterPosition() const override { return position_; }
	void SetCenterPosition(const K4E::Vector3& pos) override { position_ = pos; }

private:
	std::unique_ptr<K4E::Object3D> object3d_;
	K4E::Vector3 position_{};
	K4E::Vector3 basePosition_{};
	K4E::Vector3 rotation_{};
	K4E::Vector3 halfSize_{ 0.9f, 0.9f, 0.9f };
	float pickupRadius_ = 2.1f;
	float floatTimer_ = 0.0f;
	bool spawned_ = false;
	bool collected_ = false;
};

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
	enum class TutorialStep
	{
		None,
		CrystalExplanation,
		MovePractice,
		MouseLookPractice,
		ShootPractice,
		ReloadPractice,
		EnemyPractice,
		ItemPickupPractice,
		Completed,
	};

public: /// ---------- メンバ関数 ---------- ///

	// StageContextのステージ情報をもとに、ステージ/キャラクター/衝突/HUD/目的管理を生成する。
	void Initialize(GamePlayStageContext& stageContext);
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
	bool IsGameClearRequested() const { return isGameClear_; }

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
	bool IsBossIntroActive() const { return bossIntroController_.IsRunning(); }
	bool IsBossIntroGameplayPaused() const { return bossIntroController_.IsGameplayPaused(); }
	// カメラ演出中は通常3D/HUDを止め、専用描画だけに切り替える。
	bool IsBossIntroPresentationActive() const { return bossIntroController_.IsGameplayPaused(); }

private: /// ---------- メンバ関数 ---------- ///

	void CollisionUpdate();
	void InitializeGameplayPhysicsTest();
	void ResetGameplayPhysicsTestObject();
	void UpdateGameplayPhysicsTest(float deltaTime);
	void DrawGameplayPhysicsTest();
	void DrawGameplayPhysicsTestImGui();
	void SyncGameplayPhysicsTestCollider();
	void BindGameplayPhysicsStageColliders();
	void UnbindGameplayPhysicsStageColliders();
	void RegisterPlayerPhysicsGroundCheck();
	void UnregisterPlayerPhysicsGroundCheck();
	void UpdatePlayerPhysicsGroundCheck();
	void SyncPlayerPhysicsGroundCollider(Player& player);
	void ApplyPlayerPhysicsCorrection(Player& player);
	bool EvaluatePlayerPhysicsGrounded();
	void InitializeGameplayPhysicsTriggerTest();
	void SetGameplayPhysicsTriggerTestEnabled(bool enabled);
	void RegisterGameplayPhysicsTriggerTest();
	void UnregisterGameplayPhysicsTriggerTest();
	void ResetGameplayPhysicsTriggerTest();
	void UpdateGameplayPhysicsTriggerTest(float deltaTime);
	void DrawGameplayPhysicsTriggerTest();
	void DrawGameplayPhysicsTriggerTestImGui();
	void SyncGameplayPhysicsTriggerTarget();
	void RegisterGameplayPhysicsEventListener();
	void UnregisterGameplayPhysicsEventListener();
	void SyncGameplayPhysicsBulletTriggerTargets();
	void UnregisterGameplayPhysicsBulletTriggerTargets();
	void ApplyGameplayPhysicsParameterSettings();
	void UpdateCollisionSystemPolicyFromGameplayFlags();
	void DrawCollisionSystemPolicyImGui();
	void UpdateShadowLightViewProjection();
	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const;
	bool IsSightBlocked(const K4E::Segment& seg) const;
	void UpdateCrystalBossSpawnProgress();
	void UpdateBossIntro(float deltaTime);
	void UpdateBossIntroPausedWorld(float deltaTime);
	void SpawnGuardianBoss(bool registerCollider);
	void RegisterGuardianBossCollider();
	void AlignPlayerViewToBossAfterIntro(Player& player);
	void UpdateBossGuideHud(Player& player, bool bossBattleActive);
	void StartStage1ObjectiveGuide();
	void UpdateStage1ObjectiveIntro(float deltaTime);
	void FinishStage1ObjectiveIntro();
	void AdvanceStage1TutorialStep();
	bool IsTutorialPlaying() const;
	bool IsGameplayBlocked() const;
	bool AllowsPlayerMove() const;
	bool AllowsPlayerShoot() const;
	bool AllowsReload() const;
	bool AllowsTutorialEnemyUpdate() const;
	void ApplyStage1TutorialPlayerRestrictions();
	void SpawnStage1TutorialEnemy();
	void ClearStage1TutorialEnemy();
	void SpawnStage1TutorialItems();
	void UpdateStage1TutorialHud(float tutorialAlpha);
	void AlignPlayerViewToFirstCrystal(Player& player);
	void UpdateStage1ObjectiveGuideHud(bool bossBattleActive);
	void ResetBossIntroForDebug();
	void UpdateBossClearProgress(float deltaTime);
	void SpawnClearItem(const K4E::Vector3& bossPosition);
	void CollectClearItem();


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
	CrystalManager crystalManager_;
	AimTargetDetector aimTargetDetector_;
	std::unique_ptr<GuardianBoss> guardianBoss_;
	std::unique_ptr<BossClearItem> clearItem_;
	std::unique_ptr<K4E::Object3D> physicsTestObject_;
	std::unique_ptr<PhysicsTestBullet> physicsTestBullet_;
	std::unique_ptr<K4E::Object3D> physicsTriggerTargetObject_;
	std::unique_ptr<GameplayPhysicsEventHandler> gameplayPhysicsEventHandler_;

	K4E::PhysicsWorld gameplayPhysicsWorld_{}; // 本編接続前の明示ONテスト用PhysicsWorld
	K4E::CollisionSystemPolicy collisionSystemPolicy_{}; // 段階移行中に旧判定と新Physics判定を切り替える担当表
	K4E::PhysicsParameterBridge gameplayPhysicsParameterBridge_{}; // ParameterManagerとGameplay側PhysicsWorldの橋渡し
	K4E::PhysicsDebugDraw gameplayPhysicsDebugDraw_{}; // Gameplay側PhysicsWorldの共通Debug可視化
	K4E::StagePhysicsBinder gameplayStagePhysicsBinder_{}; // StageCollider登録確認用Binder
	K4E::Rigidbody physicsTestRigidbody_{}; // PhysicsTestObject用Rigidbody
	K4E::Collider physicsTestCollider_{}; // PhysicsTestObject用Collider
	K4E::Rigidbody playerGroundRigidbody_{}; // Player床判定確認用Kinematic Rigidbody
	K4E::Collider playerGroundCollider_{}; // Player床判定確認用Collider
	K4E::Rigidbody physicsTriggerTargetRigidbody_{}; // TriggerEvent確認用ターゲットRigidbody
	K4E::Collider physicsTriggerTargetCollider_{}; // TriggerEvent確認用ターゲットCollider
	K4E::Vector3 physicsTestPosition_{}; // PhysicsTestObjectの現在位置
	K4E::Vector3 physicsTestInitialPosition_{ 0.0f, 8.0f, 0.0f }; // Reset時の初期位置
	K4E::Vector3 physicsTestHalfSize_{ 0.5f, 0.5f, 0.5f }; // テスト用AABB半サイズ
	K4E::Vector3 playerGroundColliderPosition_{}; // Player床判定用Collider中心
	K4E::Vector3 playerPositionBeforePhysics_{}; // PhysicsWorld同期前のPlayer位置
	K4E::Vector3 playerPositionAfterPhysics_{}; // PhysicsWorld補正反映後のPlayer位置
	K4E::Vector3 playerPhysicsCorrectionDelta_{}; // PhysicsWorldから受け取ったPlayer補正量
	K4E::Vector3 physicsTriggerTargetPosition_{}; // TriggerEvent確認用ターゲット位置
	K4E::Vector3 physicsTriggerTargetHalfSize_{ 0.75f, 0.75f, 0.75f }; // TriggerEvent確認用ターゲット半サイズ
	K4E::Vector3 physicsTestBulletSpawnPosition_{}; // TriggerEvent確認用テスト弾の初期位置
	K4E::Vector3 physicsTestBulletInitialVelocity_{ 0.0f, 0.0f, 12.0f }; // TriggerEvent確認用テスト弾の初期速度
	K4E::Vector3 playerGroundColliderHalfSize_{ 0.5f, 1.0f, 0.5f }; // Player床判定用AABB半サイズ
	K4E::Vector3 playerGroundColliderOffset_{ 0.0f, 0.95f, 0.0f }; // 足元に少し重なる床判定用オフセット
	bool enableGameplayPhysicsTest_ = false; // 本編上でPhysicsWorldテストを実行するか
	bool enablePlayerPhysicsGroundCheck_ = false; // Player床判定だけをPhysicsWorldから取得するか
	bool enablePlayerPhysicsDepenetration_ = false; // Player壁押し戻しだけをPhysicsWorldから受け取るか
	bool enableGameplayPhysicsTriggerTest_ = false; // TriggerEventを本編側で受け取る入口テストを実行するか
	bool enableGameplayPhysicsDebugDraw_ = false; // Gameplay側PhysicsWorldのDebug可視化を行うか
	bool usePhysicsForPlayerStage_ = false; // Player vs StageをPhysicsWorld側へ寄せる段階移行フラグ
	bool usePhysicsForPlayerGround_ = false; // Player床判定をPhysicsWorld側へ寄せる段階移行フラグ
	bool usePhysicsForPlayerDepenetration_ = false; // Player押し戻しをPhysicsWorld側へ寄せる段階移行フラグ
	bool usePhysicsForTriggerTest_ = false; // テストTriggerをPhysicsWorld側イベントで扱う段階移行フラグ
	bool usePhysicsForBulletTrigger_ = false; // 既存Bullet Trigger移行予定を明示する段階移行フラグ
	bool usePhysicsForEnemyStage_ = false; // Enemy vs Stage移行予定を明示する段階移行フラグ
	bool applyPlayerPhysicsCorrectionXZ_ = true; // Player補正のXZ成分を反映するか
	bool applyPlayerPhysicsCorrectionY_ = false; // Player補正のY成分を反映するか
	bool gameplayPhysicsStageBound_ = false; // StageColliderをPhysicsWorldへ登録済みか
	bool playerGroundColliderRegistered_ = false; // Player床判定用ColliderをPhysicsWorldへ登録済みか
	bool gameplayPhysicsTriggerTestRegistered_ = false; // TriggerEvent確認用ColliderをPhysicsWorldへ登録済みか
	bool gameplayPhysicsEventListenerRegistered_ = false; // TriggerEvent確認用ListenerをPhysicsWorldへ登録済みか
	std::vector<K4E::Collider*> physicsBulletTargetColliders_{}; // 実Bullet Trigger確認用にPhysicsWorldへ登録中のEnemy/Boss Collider
	bool playerPhysicsGrounded_ = false; // PhysicsWorld由来のPlayer床判定
	size_t playerStageContactCount_ = 0; // Player vs StageのContact数
	float playerCorrectionClamp_ = 1.0f; // Playerへ反映する物理補正量の1フレーム上限

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
	K4E::Vector3 bossSpawnPosition_{ 0.0f, 2.25f, 30.0f };
	BossIntroController bossIntroController_;
	bool stage1BeginnerBalanceEnabled_ = false;
	bool stage1ObjectiveIntroActive_ = false;
	float stage1ObjectiveIntroTimer_ = 0.0f;
	float stage1ObjectiveIntroFadeInTime_ = 0.4f;
	float stage1ObjectiveIntroHoldTime_ = 4.0f;
	float stage1ObjectiveIntroFadeOutTime_ = 0.8f;
	float stage1ItemIntroFadeInTime_ = 0.4f;
	float stage1ItemIntroHoldTime_ = 4.0f;
	float stage1ItemIntroFadeOutTime_ = 0.8f;
	TutorialStep stage1TutorialStep_ = TutorialStep::None;
	float stage1MoveProgress_ = 0.0f;
	float stage1MouseLookProgress_ = 0.0f;
	float stage1ShootProgress_ = 0.0f;
	int stage1ShootCount_ = 0;
	K4E::Vector3 stage1MovePreviousPlayerPosition_{};
	EnemyBase* stage1TutorialEnemy_ = nullptr;
	bool stage1TutorialEnemySpawned_ = false;
	bool stage1TutorialItemSpawned_ = false;
	int stage1TutorialItemsCollected_ = 0;
	bool stage1SavedEnemyDeathDropEnabled_ = true;
	bool stage1ReloadStarted_ = false;
	bool stage1ReloadWasReloading_ = false;
	bool stage1TutorialCompletionNotified_ = false;
	float stage1TutorialCompleteTimer_ = 0.0f;
	float stage1TutorialCompleteHoldTime_ = 1.4f;
	K4E::Vector3 stage1ObjectiveSavedCameraRotation_{};
	bool bossSpawned_ = false;
	bool bossColliderRegistered_ = false;
	bool bossSpawnConditionMet_ = false;
	bool bossDefeated_ = false;
	bool clearItemSpawned_ = false;
	bool clearItemCollected_ = false;
	bool isGameClear_ = false;
};
