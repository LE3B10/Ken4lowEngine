#pragma once

#include "CollisionSystemPolicy.h"
#include "PhysicsDebugDraw.h"
#include "PhysicsParameterBridge.h"
#include "PhysicsWorld.h"
#include "Rigidbody.h"
#include "StagePhysicsBinder.h"
#include "Object3D.h"
#include "PhysicsTestBullet.h"
#include "GameplayPhysicsEventHandler.h"

#include <functional>
#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	class Stage;
}

namespace K4E = ::Ken4lowEngine;

class BulletManager;
class CharacterWorld;
class CollisionManager;
class EnemyBase;
class GuardianBoss;
class Player;

/// -------------------------------------------------------------
/// Gameplay中のPhysicsWorld移行テストとDebug表示を管理するクラス。
///
/// GamePlayWorldからPhysics検証用のCollider登録、Trigger確認、Player床判定、
/// ImGui表示を切り離し、本編進行の処理量を減らす。
/// -------------------------------------------------------------
class GameplayPhysicsDebugController
{
public:
	/// GamePlayWorldが所有している実体への参照を受け取り、Physics検証処理だけを担当する。
	struct Dependencies
	{
		CharacterWorld* characters = nullptr;
		BulletManager* bulletManager = nullptr;
		CollisionManager* collisionManager = nullptr;
		K4E::Stage* stage = nullptr;
		std::function<GuardianBoss*()> getBoss;
		std::function<bool()> isBossColliderRegistered;
	};

	GameplayPhysicsDebugController() = default;
	~GameplayPhysicsDebugController();

	void Initialize(const Dependencies& deps);
	void Finalize();
	void Update(const Dependencies& deps, float deltaTime);
	void Draw();
	void DrawImGui(const Dependencies& deps);

private:
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

	Player* GetPlayer() const;
	std::vector<EnemyBase*> GetEnemies() const;
	GuardianBoss* GetBoss() const;
	bool IsBossColliderRegistered() const;

private:
	Dependencies deps_{};

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
};
