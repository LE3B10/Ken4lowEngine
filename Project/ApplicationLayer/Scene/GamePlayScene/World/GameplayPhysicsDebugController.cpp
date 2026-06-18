#define NOMINMAX
#include "GameplayPhysicsDebugController.h"

#include "BulletManager.h"
#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "GameplayPhysicsEventHandler.h"
#include "PhysicsTestBullet.h"
#include "Player.h"
#include "Stage.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "Wireframe.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;


namespace
{
	static constexpr uint32_t kPhysicsLayerStage = 0u;
	static constexpr uint32_t kPhysicsLayerTestObject = 1u;
	static constexpr uint32_t kPhysicsLayerPlayer = 2u;
	static constexpr uint32_t kPhysicsLayerTestBullet = 3u;
	static constexpr uint32_t kPhysicsLayerTestTarget = 4u;
	static constexpr uint32_t kPhysicsLayerPlayerBullet = 5u;
	static constexpr uint32_t kPhysicsLayerEnemy = 6u;
	static constexpr uint32_t kPhysicsLayerBoss = 7u;

	const char* ToCollisionResponseName(K4E::CollisionResponseType response)
	{
		switch (response)
		{
		case K4E::CollisionResponseType::Ignore:
			return "Ignore";
		case K4E::CollisionResponseType::Trigger:
			return "Trigger";
		case K4E::CollisionResponseType::Block:
			return "Block";
		default:
			return "Unknown";
		}
	}
}


GameplayPhysicsDebugController::~GameplayPhysicsDebugController() = default;

void GameplayPhysicsDebugController::Initialize(const Dependencies& deps)
{
	deps_ = deps;
	InitializeGameplayPhysicsTest();
	InitializeGameplayPhysicsTriggerTest();
	gameplayPhysicsParameterBridge_.Initialize();
	gameplayPhysicsParameterBridge_.RegisterAppliers(this, [this]() { ApplyGameplayPhysicsParameterSettings(); });
	ApplyGameplayPhysicsParameterSettings();
}

void GameplayPhysicsDebugController::Finalize()
{
	UnregisterGameplayPhysicsBulletTriggerTargets();
	UnregisterPlayerPhysicsGroundCheck();
	UnregisterGameplayPhysicsTriggerTest();
	gameplayPhysicsParameterBridge_.Finalize(this);
	UnbindGameplayPhysicsStageColliders();
	physicsTestObject_.reset();
	physicsTestBullet_.reset();
	physicsTriggerTargetObject_.reset();
	deps_ = {};
}

void GameplayPhysicsDebugController::Update(const Dependencies& deps, float deltaTime)
{
	deps_ = deps;
	UpdateGameplayPhysicsTest(deltaTime);
}

void GameplayPhysicsDebugController::Draw()
{
	DrawGameplayPhysicsTest();
}

void GameplayPhysicsDebugController::DrawImGui(const Dependencies& deps)
{
	deps_ = deps;
	DrawGameplayPhysicsTestImGui();
}

Player* GameplayPhysicsDebugController::GetPlayer() const
{
	return deps_.characters ? deps_.characters->GetPlayer() : nullptr;
}

std::vector<EnemyBase*> GameplayPhysicsDebugController::GetEnemies() const
{
	return deps_.characters ? deps_.characters->GetEnemyRawList() : std::vector<EnemyBase*>{};
}

GuardianBoss* GameplayPhysicsDebugController::GetBoss() const
{
	return deps_.getBoss ? deps_.getBoss() : nullptr;
}

bool GameplayPhysicsDebugController::IsBossColliderRegistered() const
{
	return deps_.isBossColliderRegistered ? deps_.isBossColliderRegistered() : false;
}

void GameplayPhysicsDebugController::InitializeGameplayPhysicsTest()
{
	// 本編挙動へ影響しない明示ONの物理テストとして、独立したPhysicsWorldとTestObjectを準備する。
	gameplayPhysicsWorld_.SetUseFixedStep(true);
	gameplayPhysicsWorld_.SetPositionSolveEnabled(true);
	gameplayPhysicsWorld_.SetFrictionSolveEnabled(true);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerStage, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerStage, kPhysicsLayerTestObject, K4E::CollisionResponseType::Block);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerStage, kPhysicsLayerPlayer, K4E::CollisionResponseType::Block);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestObject, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestObject, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestBullet, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestTarget, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerTestObject, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerTestTarget, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerEnemy, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerBoss, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerStage, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerTestObject, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerTestBullet, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerTestTarget, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerPlayerBullet, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerEnemy, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerBoss, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);

	if (deps_.bulletManager)
	{
		deps_.bulletManager->SetPhysicsTriggerWorld(&gameplayPhysicsWorld_, kPhysicsLayerPlayerBullet);
	}

	physicsTestRigidbody_.SetBodyType(K4E::BodyType::Dynamic);
	physicsTestRigidbody_.SetMass(1.0f);
	physicsTestRigidbody_.SetUseGravity(true);
	physicsTestRigidbody_.SetRestitution(0.0f);
	physicsTestRigidbody_.SetDynamicFriction(0.2f);
	physicsTestRigidbody_.SetSleepEnabled(false);

	physicsTestCollider_.SetRigidbody(&physicsTestRigidbody_);
	physicsTestCollider_.SetCollisionLayer(kPhysicsLayerTestObject);
	gameplayPhysicsWorld_.RegisterRigidbody(&physicsTestRigidbody_);
	gameplayPhysicsWorld_.RegisterCollider(&physicsTestCollider_);

	playerGroundRigidbody_.SetBodyType(K4E::BodyType::Kinematic);
	playerGroundRigidbody_.SetUseGravity(false);
	playerGroundCollider_.SetRigidbody(&playerGroundRigidbody_);
	playerGroundCollider_.SetCollisionLayer(kPhysicsLayerPlayer);

	if (auto* player = GetPlayer())
	{
		physicsTestInitialPosition_ = player->GetCenterPosition() + K4E::Vector3{ 0.0f, 8.0f, 3.0f };
	}
	else
	{
		physicsTestInitialPosition_ = { 0.0f, 8.0f, 0.0f };
	}

	physicsTestObject_ = std::make_unique<K4E::Object3D>();
	physicsTestObject_->Initialize("Test/cube.gltf");
	physicsTestObject_->SetScale(physicsTestHalfSize_ * 2.0f);
	physicsTestObject_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });

	ResetGameplayPhysicsTestObject();
}

void GameplayPhysicsDebugController::ResetGameplayPhysicsTestObject()
{
	// PhysicsTestObjectを初期位置へ戻し、速度・力・接触状態をリセットする。
	physicsTestPosition_ = physicsTestInitialPosition_;
	physicsTestRigidbody_.SetVelocity({});
	physicsTestRigidbody_.ClearForces();
	physicsTestRigidbody_.ClearFrameState();
	physicsTestRigidbody_.WakeUp();
	SyncGameplayPhysicsTestCollider();

	if (physicsTestObject_)
	{
		physicsTestObject_->SetTranslate(physicsTestPosition_);
		physicsTestObject_->Update();
	}
}

void GameplayPhysicsDebugController::InitializeGameplayPhysicsTriggerTest()
{
	// PhysicsWorldのTriggerEventを本編側で受け取るため、既存Bulletとは別のテスト弾とターゲットを準備する。
	physicsTestBullet_ = std::make_unique<PhysicsTestBullet>();
	physicsTestBullet_->Initialize(kPhysicsLayerTestBullet);

	gameplayPhysicsEventHandler_ = std::make_unique<GameplayPhysicsEventHandler>();
	gameplayPhysicsEventHandler_->Configure(physicsTestBullet_.get(), &physicsTriggerTargetCollider_);

	physicsTriggerTargetRigidbody_.SetBodyType(K4E::BodyType::Static);
	physicsTriggerTargetRigidbody_.SetUseGravity(false);
	physicsTriggerTargetCollider_.SetRigidbody(&physicsTriggerTargetRigidbody_);
	physicsTriggerTargetCollider_.SetCollisionLayer(kPhysicsLayerTestTarget);
	physicsTriggerTargetCollider_.SetTrigger(true);
	physicsTriggerTargetCollider_.SetEnabled(false);

	physicsTriggerTargetObject_ = std::make_unique<K4E::Object3D>();
	physicsTriggerTargetObject_->Initialize("Test/cube.gltf");
	physicsTriggerTargetObject_->SetScale(physicsTriggerTargetHalfSize_ * 2.0f);
	physicsTriggerTargetObject_->SetColor({ 0.2f, 1.0f, 0.3f, 1.0f });
	physicsTriggerTargetObject_->Update();

	ResetGameplayPhysicsTriggerTest();
}

void GameplayPhysicsDebugController::SetGameplayPhysicsTriggerTestEnabled(bool enabled)
{
	enableGameplayPhysicsTriggerTest_ = enabled;
	usePhysicsForTriggerTest_ = enabled;
	UpdateCollisionSystemPolicyFromGameplayFlags();
	if (enableGameplayPhysicsTriggerTest_)
	{
		RegisterGameplayPhysicsTriggerTest();
		ResetGameplayPhysicsTriggerTest();
	}
	else
	{
		UnregisterGameplayPhysicsTriggerTest();
	}
}

void GameplayPhysicsDebugController::RegisterGameplayPhysicsTriggerTest()
{
	// TriggerEvent確認用Collider/Listenerを登録し、PhysicsWorldから本編側へイベントを通知できるようにする。
	if (!physicsTestBullet_ || !gameplayPhysicsEventHandler_)
	{
		return;
	}

	if (!gameplayPhysicsTriggerTestRegistered_)
	{
		gameplayPhysicsWorld_.RegisterRigidbody(physicsTestBullet_->GetRigidbody());
		gameplayPhysicsWorld_.RegisterCollider(physicsTestBullet_->GetCollider());
		gameplayPhysicsWorld_.RegisterRigidbody(&physicsTriggerTargetRigidbody_);
		gameplayPhysicsWorld_.RegisterCollider(&physicsTriggerTargetCollider_);
		gameplayPhysicsTriggerTestRegistered_ = true;
	}
	RegisterGameplayPhysicsEventListener();
}

void GameplayPhysicsDebugController::UnregisterGameplayPhysicsTriggerTest()
{
	// 破棄済みポインタ参照を防ぐため、Scene終了や無効化時にListenerとCollider登録を解除する。
	if (gameplayPhysicsEventListenerRegistered_ && gameplayPhysicsEventHandler_)
	{
		if (!usePhysicsForBulletTrigger_)
		{
			UnregisterGameplayPhysicsEventListener();
		}
	}
	if (gameplayPhysicsTriggerTestRegistered_)
	{
		if (physicsTestBullet_)
		{
			gameplayPhysicsWorld_.UnregisterCollider(physicsTestBullet_->GetCollider());
			gameplayPhysicsWorld_.UnregisterRigidbody(physicsTestBullet_->GetRigidbody());
		}
		gameplayPhysicsWorld_.UnregisterCollider(&physicsTriggerTargetCollider_);
		gameplayPhysicsWorld_.UnregisterRigidbody(&physicsTriggerTargetRigidbody_);
		gameplayPhysicsTriggerTestRegistered_ = false;
	}

	physicsTriggerTargetCollider_.SetEnabled(false);
	if (physicsTestBullet_)
	{
		physicsTestBullet_->Kill();
	}
}

void GameplayPhysicsDebugController::RegisterGameplayPhysicsEventListener()
{
	// PhysicsWorldのTriggerEventをGameplay側へ渡すため、テスト弾/実Bullet共通のListenerを登録する。
	if (!gameplayPhysicsEventListenerRegistered_ && gameplayPhysicsEventHandler_)
	{
		gameplayPhysicsWorld_.AddPhysicsEventListener(gameplayPhysicsEventHandler_.get());
		gameplayPhysicsEventListenerRegistered_ = true;
	}
}

void GameplayPhysicsDebugController::UnregisterGameplayPhysicsEventListener()
{
	// 破棄済みポインタ参照を防ぐため、不要になったListenerはPhysicsWorldから外す。
	if (gameplayPhysicsEventListenerRegistered_ && gameplayPhysicsEventHandler_)
	{
		gameplayPhysicsWorld_.RemovePhysicsEventListener(gameplayPhysicsEventHandler_.get());
		gameplayPhysicsEventListenerRegistered_ = false;
	}
}

void GameplayPhysicsDebugController::ResetGameplayPhysicsTriggerTest()
{
	// テスト弾とターゲットをPlayer前方へ置き、TriggerEnterの再確認ができる状態に戻す。
	K4E::Vector3 basePosition{ 0.0f, 2.0f, 0.0f };
	if (auto* player = GetPlayer())
	{
		basePosition = player->GetCenterPosition();
		basePosition.y += 1.0f;
	}

	physicsTestBulletSpawnPosition_ = basePosition + K4E::Vector3{ 0.0f, 0.0f, 2.0f };
	physicsTriggerTargetPosition_ = basePosition + K4E::Vector3{ 0.0f, 0.0f, 8.0f };
	SyncGameplayPhysicsTriggerTarget();

	if (physicsTestBullet_)
	{
		physicsTestBullet_->Reset(physicsTestBulletSpawnPosition_, physicsTestBulletInitialVelocity_);
	}
	if (gameplayPhysicsEventHandler_)
	{
		gameplayPhysicsEventHandler_->Reset();
	}
}

void GameplayPhysicsDebugController::UpdateGameplayPhysicsTriggerTest(float deltaTime)
{
	// 既存Bulletへ触れず、PhysicsWorldのTriggerEvent確認用テスト弾だけを更新する。
	if (!enableGameplayPhysicsTriggerTest_)
	{
		return;
	}

	if (!gameplayPhysicsTriggerTestRegistered_)
	{
		RegisterGameplayPhysicsTriggerTest();
	}

	if (physicsTestBullet_)
	{
		physicsTestBullet_->Update(deltaTime);
	}
	SyncGameplayPhysicsTriggerTarget();
}

void GameplayPhysicsDebugController::DrawGameplayPhysicsTriggerTest()
{
#ifdef _DEBUG
	if (!enableGameplayPhysicsTriggerTest_)
	{
		return;
	}

	if (physicsTriggerTargetObject_)
	{
		const bool hit = gameplayPhysicsEventHandler_ && gameplayPhysicsEventHandler_->HasTriggerHit();
		physicsTriggerTargetObject_->SetColor(hit ? K4E::Vector4{ 1.0f, 0.2f, 0.2f, 1.0f } : K4E::Vector4{ 0.2f, 1.0f, 0.3f, 1.0f });
		physicsTriggerTargetObject_->SetTranslate(physicsTriggerTargetPosition_);
		physicsTriggerTargetObject_->Update();
		physicsTriggerTargetObject_->Draw();
	}
	if (physicsTestBullet_)
	{
		physicsTestBullet_->Draw();
	}

	K4E::Wireframe::GetInstance()->DrawAABB(
		physicsTriggerTargetCollider_.GetAABB(),
		gameplayPhysicsEventHandler_ && gameplayPhysicsEventHandler_->HasTriggerHit()
		? K4E::Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
	: K4E::Vector4{ 0.2f, 1.0f, 0.3f, 1.0f });
	if (physicsTestBullet_)
	{
		K4E::Wireframe::GetInstance()->DrawAABB(
			physicsTestBullet_->GetCollider()->GetAABB(),
			physicsTestBullet_->IsAlive() ? K4E::Vector4{ 1.0f, 0.9f, 0.15f, 1.0f } : K4E::Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
	}
#endif
}

void GameplayPhysicsDebugController::DrawGameplayPhysicsTriggerTestImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Gameplay Physics Trigger Test");

	bool enableTriggerTest = enableGameplayPhysicsTriggerTest_;
	if (ImGui::Checkbox("Enable Trigger Test", &enableTriggerTest))
	{
		SetGameplayPhysicsTriggerTestEnabled(enableTriggerTest);
	}
	if (ImGui::Button("Spawn / Reset Test Bullet"))
	{
		RegisterGameplayPhysicsTriggerTest();
		enableGameplayPhysicsTriggerTest_ = true;
		ResetGameplayPhysicsTriggerTest();
	}

	const K4E::CollisionResponseType response =
		gameplayPhysicsWorld_.GetResponseMatrix().GetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestTarget);
	const K4E::Vector3 bulletPosition = physicsTestBullet_ ? physicsTestBullet_->GetPosition() : K4E::Vector3{};

	ImGui::Text("Bullet Alive: %s", physicsTestBullet_ && physicsTestBullet_->IsAlive() ? "true" : "false");
	ImGui::Text("Trigger Enter Count: %d", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetTriggerEnterCount() : 0);
	ImGui::Text("Latest Trigger Event: %s", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetLatestTriggerEvent().c_str() : "None");
	ImGui::Text("Bullet Position: %.3f, %.3f, %.3f", bulletPosition.x, bulletPosition.y, bulletPosition.z);
	ImGui::Text("Target Position: %.3f, %.3f, %.3f", physicsTriggerTargetPosition_.x, physicsTriggerTargetPosition_.y, physicsTriggerTargetPosition_.z);
	ImGui::Text("Response Type: %s", ToCollisionResponseName(response));
	ImGui::Text("Trigger Test Registered: %s", gameplayPhysicsTriggerTestRegistered_ ? "true" : "false");
#endif
}

void GameplayPhysicsDebugController::SyncGameplayPhysicsTriggerTarget()
{
	// TriggerEvent確認用ターゲットの表示位置とPhysicsWorld Colliderを同期する。
	physicsTriggerTargetCollider_.SetEnabled(enableGameplayPhysicsTriggerTest_);
	physicsTriggerTargetCollider_.SetAABB({
		physicsTriggerTargetPosition_ - physicsTriggerTargetHalfSize_,
		physicsTriggerTargetPosition_ + physicsTriggerTargetHalfSize_,
		});

	if (physicsTriggerTargetObject_)
	{
		physicsTriggerTargetObject_->SetTranslate(physicsTriggerTargetPosition_);
		physicsTriggerTargetObject_->Update();
	}
}

void GameplayPhysicsDebugController::SyncGameplayPhysicsBulletTriggerTargets()
{
	if (!usePhysicsForBulletTrigger_)
	{
		UnregisterGameplayPhysicsBulletTriggerTargets();
		return;
	}

	RegisterGameplayPhysicsEventListener();
	BindGameplayPhysicsStageColliders();

	std::vector<K4E::Collider*> desiredTargets{};
	const std::vector<EnemyBase*> enemies = GetEnemies();
	desiredTargets.reserve(enemies.size() + 1);
	for (EnemyBase* enemy : enemies)
	{
		if (!enemy || enemy->IsDead() || enemy->IsRemovable())
		{
			continue;
		}

		enemy->SetCollisionLayer(kPhysicsLayerEnemy);
		desiredTargets.push_back(enemy);
	}
	if (auto* boss = GetBoss(); boss && IsBossColliderRegistered() && boss->IsAlive())
	{
		boss->SetCollisionLayer(kPhysicsLayerBoss);
		desiredTargets.push_back(boss);
	}

	for (K4E::Collider* registered : physicsBulletTargetColliders_)
	{
		if (std::find(desiredTargets.begin(), desiredTargets.end(), registered) == desiredTargets.end())
		{
			// 破棄済みEnemy/Boss Collider参照を防ぐため、不要になった実Bullet Trigger対象を解除する。
			gameplayPhysicsWorld_.UnregisterCollider(registered);
		}
	}
	for (K4E::Collider* target : desiredTargets)
	{
		gameplayPhysicsWorld_.RegisterCollider(target);
	}
	physicsBulletTargetColliders_ = std::move(desiredTargets);
}

void GameplayPhysicsDebugController::UnregisterGameplayPhysicsBulletTriggerTargets()
{
	for (K4E::Collider* collider : physicsBulletTargetColliders_)
	{
		// 破棄済みEnemy/Boss Collider参照を防ぐため、PhysicsWorldからTarget登録を解除する。
		gameplayPhysicsWorld_.UnregisterCollider(collider);
	}
	physicsBulletTargetColliders_.clear();
	if (deps_.bulletManager)
	{
		deps_.bulletManager->SetUsePhysicsTriggerForNormalBullets(false);
	}
	if (!enableGameplayPhysicsTriggerTest_)
	{
		UnregisterGameplayPhysicsEventListener();
	}
}

void GameplayPhysicsDebugController::UpdateGameplayPhysicsTest(float deltaTime)
{
	// 明示ONの確認機能がある場合だけ、本編とは別PhysicsWorldでテスト物体/Player床判定を更新する。
	if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_ && !enableGameplayPhysicsTriggerTest_ && !usePhysicsForBulletTrigger_)
	{
		return;
	}

	if (enableGameplayPhysicsTest_)
	{
		physicsTestPosition_ += physicsTestRigidbody_.GetVelocity() * deltaTime;
		SyncGameplayPhysicsTestCollider();
	}
	UpdateGameplayPhysicsTriggerTest(deltaTime);
	SyncGameplayPhysicsBulletTriggerTargets();
	UpdatePlayerPhysicsGroundCheck();

	gameplayPhysicsWorld_.Update(deltaTime);

	if (enableGameplayPhysicsTest_)
	{
		physicsTestPosition_ = physicsTestCollider_.GetCenterPosition();

		if (physicsTestObject_)
		{
			physicsTestObject_->SetTranslate(physicsTestPosition_);
			physicsTestObject_->Update();
		}
	}

	playerPhysicsGrounded_ = EvaluatePlayerPhysicsGrounded();
	playerGroundRigidbody_.SetGrounded(playerPhysicsGrounded_);
	if (auto* player = GetPlayer())
	{
		if (enablePlayerPhysicsDepenetration_)
		{
			ApplyPlayerPhysicsCorrection(*player);
		}
		// PhysicsWorld側の接地状態をPlayerへ反映する。既存の移動/ジャンプ判定にはまだ使わない。
		player->SetGroundedByPhysics(enablePlayerPhysicsGroundCheck_ && playerPhysicsGrounded_);
	}
}

void GameplayPhysicsDebugController::DrawGameplayPhysicsTest()
{
#ifdef _DEBUG
	// テスト有効時だけ本編ステージ上のPhysicsTestObjectとColliderを可視化する。
	if (!enableGameplayPhysicsTest_ && !enableGameplayPhysicsTriggerTest_ && !enableGameplayPhysicsDebugDraw_)
	{
		return;
	}

	if (enableGameplayPhysicsTest_ && physicsTestObject_)
	{
		physicsTestObject_->Draw();
	}

	if (enableGameplayPhysicsTest_)
	{
		K4E::Wireframe::GetInstance()->DrawAABB(
			physicsTestCollider_.GetAABB(),
			physicsTestRigidbody_.IsGrounded() ? K4E::Vector4{ 0.1f, 1.0f, 0.2f, 1.0f } : K4E::Vector4{ 0.2f, 0.9f, 1.0f, 1.0f });
	}
	DrawGameplayPhysicsTriggerTest();
	if (enableGameplayPhysicsDebugDraw_)
	{
		// Gameplay側でも共通Debug描画を使い、Player床判定/押し戻し/TriggerEventの調査に使う。
		gameplayPhysicsDebugDraw_.Draw(gameplayPhysicsWorld_);
	}
#endif
}

void GameplayPhysicsDebugController::DrawGameplayPhysicsTestImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Gameplay Physics Test");

	bool enable = enableGameplayPhysicsTest_;
	if (ImGui::Checkbox("Enable Gameplay Physics Test", &enable))
	{
		enableGameplayPhysicsTest_ = enable;
		usePhysicsForPlayerStage_ = enableGameplayPhysicsTest_ || enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_;
		if (enableGameplayPhysicsTest_)
		{
			BindGameplayPhysicsStageColliders();
			ResetGameplayPhysicsTestObject();
		}
		else
		{
			if (!enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_)
			{
				UnbindGameplayPhysicsStageColliders();
			}
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}
	bool enablePlayerGroundCheck = enablePlayerPhysicsGroundCheck_;
	if (ImGui::Checkbox("Enable Player Physics Ground Check", &enablePlayerGroundCheck))
	{
		enablePlayerPhysicsGroundCheck_ = enablePlayerGroundCheck;
		usePhysicsForPlayerGround_ = enablePlayerPhysicsGroundCheck_;
		usePhysicsForPlayerStage_ = enableGameplayPhysicsTest_ || enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_;
		if (enablePlayerPhysicsGroundCheck_)
		{
			BindGameplayPhysicsStageColliders();
			RegisterPlayerPhysicsGroundCheck();
		}
		else
		{
			if (!enablePlayerPhysicsDepenetration_)
			{
				UnregisterPlayerPhysicsGroundCheck();
			}
			if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsDepenetration_)
			{
				UnbindGameplayPhysicsStageColliders();
			}
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}
	bool enablePlayerDepenetration = enablePlayerPhysicsDepenetration_;
	if (ImGui::Checkbox("Enable Player Physics Depenetration", &enablePlayerDepenetration))
	{
		enablePlayerPhysicsDepenetration_ = enablePlayerDepenetration;
		usePhysicsForPlayerDepenetration_ = enablePlayerPhysicsDepenetration_;
		usePhysicsForPlayerStage_ = enableGameplayPhysicsTest_ || enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_;
		if (enablePlayerPhysicsDepenetration_)
		{
			BindGameplayPhysicsStageColliders();
			RegisterPlayerPhysicsGroundCheck();
		}
		else
		{
			playerPhysicsCorrectionDelta_ = {};
			if (!enablePlayerPhysicsGroundCheck_)
			{
				UnregisterPlayerPhysicsGroundCheck();
			}
			if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsGroundCheck_)
			{
				UnbindGameplayPhysicsStageColliders();
			}
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}
	ImGui::Checkbox("Apply XZ Correction", &applyPlayerPhysicsCorrectionXZ_);
	ImGui::Checkbox("Apply Y Correction", &applyPlayerPhysicsCorrectionY_);
	if (ImGui::Checkbox("Use Physics For Normal Bullet Trigger", &usePhysicsForBulletTrigger_))
	{
		// PhysicsWorld移行済みBulletの二重処理を防ぐため、通常弾だけTriggerEvent経路へ切り替える。
		if (deps_.bulletManager)
		{
			deps_.bulletManager->SetUsePhysicsTriggerForNormalBullets(usePhysicsForBulletTrigger_);
		}
		if (usePhysicsForBulletTrigger_)
		{
			BindGameplayPhysicsStageColliders();
			RegisterGameplayPhysicsEventListener();
		}
		else
		{
			UnregisterGameplayPhysicsBulletTriggerTargets();
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}

	if (ImGui::Button("Bind Stage Colliders"))
	{
		BindGameplayPhysicsStageColliders();
	}
	ImGui::SameLine();
	if (ImGui::Button("Unbind Stage Colliders"))
	{
		UnbindGameplayPhysicsStageColliders();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Physics Test Object"))
	{
		ResetGameplayPhysicsTestObject();
	}

	const K4E::Vector3 velocity = physicsTestRigidbody_.GetVelocity();
	Player* player = GetPlayer();
	const K4E::Vector3 playerPosition = player ? player->GetCenterPosition() : K4E::Vector3{};
	ImGui::Text("Stage Binder Bound: %s", gameplayStagePhysicsBinder_.IsBound() ? "true" : "false");
	ImGui::Text("Bound Stage Collider Count: %zu", gameplayStagePhysicsBinder_.GetBoundColliderCount());
	ImGui::Text("PhysicsWorld Collider Count: %zu", gameplayPhysicsWorld_.GetColliderCount());
	ImGui::Text("Contact Count: %zu", gameplayPhysicsWorld_.GetContacts().size());
	ImGui::Text("IsGrounded: %s", physicsTestRigidbody_.IsGrounded() ? "true" : "false");
	ImGui::Text("Position: %.3f, %.3f, %.3f", physicsTestPosition_.x, physicsTestPosition_.y, physicsTestPosition_.z);
	ImGui::Text("Velocity: %.3f, %.3f, %.3f", velocity.x, velocity.y, velocity.z);
	ImGui::SeparatorText("Player Physics Ground Check");
	ImGui::Text("Existing Grounded: %s", player ? (player->FSM_IsGrounded() ? "true" : "false") : "N/A");
	ImGui::Text("Physics Grounded: %s", player ? (player->IsGroundedByPhysics() ? "true" : "false") : "N/A");
	ImGui::Text("Player Position: %.3f, %.3f, %.3f", playerPosition.x, playerPosition.y, playerPosition.z);
	ImGui::Text("Player Position Before Physics: %.3f, %.3f, %.3f", playerPositionBeforePhysics_.x, playerPositionBeforePhysics_.y, playerPositionBeforePhysics_.z);
	ImGui::Text("Player Position After Physics: %.3f, %.3f, %.3f", playerPositionAfterPhysics_.x, playerPositionAfterPhysics_.y, playerPositionAfterPhysics_.z);
	ImGui::Text("Correction Delta: %.3f, %.3f, %.3f", playerPhysicsCorrectionDelta_.x, playerPhysicsCorrectionDelta_.y, playerPhysicsCorrectionDelta_.z);
	ImGui::Text("Player Collider Position: %.3f, %.3f, %.3f", playerGroundColliderPosition_.x, playerGroundColliderPosition_.y, playerGroundColliderPosition_.z);
	ImGui::Text("Player vs Stage Contact Count: %zu", playerStageContactCount_);
	ImGui::Text("Registered Player Collider: %s", playerGroundColliderRegistered_ ? "true" : "false");
	ImGui::SeparatorText("Gameplay Physics Bullet Trigger");
	ImGui::Text("Physics Trigger Bullet Count: %zu", deps_.bulletManager ? deps_.bulletManager->GetPhysicsTriggerBulletCount() : 0);
	ImGui::Text("Physics Trigger Bullet Hit Count: %d", deps_.bulletManager ? deps_.bulletManager->GetPhysicsTriggerHitCount() : 0);
	ImGui::Text("Handler Bullet Trigger Hit Count: %d", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetRealBulletTriggerHitCount() : 0);
	ImGui::Text("Last Bullet Trigger Hit: %s", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetLatestRealBulletTriggerHit().c_str() : "None");
	ImGui::Text("PlayerBullet vs Enemy Response: %s", ToCollisionResponseName(gameplayPhysicsWorld_.GetResponseMatrix().GetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerEnemy)));
	ImGui::Text("PlayerBullet vs Boss Response: %s", ToCollisionResponseName(gameplayPhysicsWorld_.GetResponseMatrix().GetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerBoss)));
	ImGui::SeparatorText("Gameplay Physics Debug");
	if (ImGui::Checkbox("Enable Gameplay Physics Debug Draw", &enableGameplayPhysicsDebugDraw_))
	{
		gameplayPhysicsDebugDraw_.GetSettings().drawPhysicsDebug = enableGameplayPhysicsDebugDraw_;
	}
	gameplayPhysicsDebugDraw_.DrawImGui(gameplayPhysicsWorld_);
	gameplayPhysicsParameterBridge_.DrawImGui();
	DrawCollisionSystemPolicyImGui();
	DrawGameplayPhysicsTriggerTestImGui();
#endif
}

void GameplayPhysicsDebugController::SyncGameplayPhysicsTestCollider()
{
	// TestObjectの表示位置とPhysicsWorldで判定するAABBを同期する。
	physicsTestCollider_.SetAABB({
		physicsTestPosition_ - physicsTestHalfSize_,
		physicsTestPosition_ + physicsTestHalfSize_,
		});
}

void GameplayPhysicsDebugController::BindGameplayPhysicsStageColliders()
{
	// StageCollisionBuilder由来のStageColliderを、テスト用PhysicsWorldへStatic Colliderとして登録する。
	if (!deps_.stage)
	{
		return;
	}

	std::vector<K4E::Collider*> stageColliders = deps_.stage->GetWorldColliderPointers();
	gameplayStagePhysicsBinder_.Bind(gameplayPhysicsWorld_, stageColliders);
	gameplayPhysicsStageBound_ = gameplayStagePhysicsBinder_.IsBound();
}

void GameplayPhysicsDebugController::UnbindGameplayPhysicsStageColliders()
{
	// Unbind後にStageとのContactが消えることを確認できるよう、Binder経由の登録を解除する。
	gameplayStagePhysicsBinder_.Unbind();
	gameplayPhysicsStageBound_ = false;
}

void GameplayPhysicsDebugController::RegisterPlayerPhysicsGroundCheck()
{
	// Player床判定用ColliderをPhysicsWorldへ登録する。二重登録を避け、ON時だけ参照を持たせる。
	if (playerGroundColliderRegistered_)
	{
		return;
	}

	gameplayPhysicsWorld_.RegisterRigidbody(&playerGroundRigidbody_);
	gameplayPhysicsWorld_.RegisterCollider(&playerGroundCollider_);
	playerGroundColliderRegistered_ = true;
}

void GameplayPhysicsDebugController::UnregisterPlayerPhysicsGroundCheck()
{
	// Scene終了時に破棄済みCollider参照を残さないよう、Player床判定用Colliderを解除する。
	if (!playerGroundColliderRegistered_)
	{
		return;
	}

	gameplayPhysicsWorld_.UnregisterCollider(&playerGroundCollider_);
	gameplayPhysicsWorld_.UnregisterRigidbody(&playerGroundRigidbody_);
	playerGroundColliderRegistered_ = false;
	playerStageContactCount_ = 0;
	playerPhysicsGrounded_ = false;
	playerGroundRigidbody_.SetGrounded(false);
	if (auto* player = GetPlayer())
	{
		player->SetGroundedByPhysics(false);
	}
}

void GameplayPhysicsDebugController::UpdatePlayerPhysicsGroundCheck()
{
	// Player移動は置き換えず、床判定用Colliderだけを現在のPlayer位置へ同期する。
	if (!enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_)
	{
		return;
	}

	if (!playerGroundColliderRegistered_)
	{
		RegisterPlayerPhysicsGroundCheck();
	}

	if (Player* player = GetPlayer())
	{
		SyncPlayerPhysicsGroundCollider(*player);
	}
}

void GameplayPhysicsDebugController::SyncPlayerPhysicsGroundCollider(Player& player)
{
	// 既存のPlayer移動結果をPhysicsWorldへ渡し、めり込み補正だけを受け取る。
	playerPositionBeforePhysics_ = player.GetCenterPosition();
	playerPositionAfterPhysics_ = playerPositionBeforePhysics_;
	playerPhysicsCorrectionDelta_ = {};
	playerGroundColliderPosition_ = playerPositionBeforePhysics_ + playerGroundColliderOffset_;
	playerGroundCollider_.SetAABB({
		playerGroundColliderPosition_ - playerGroundColliderHalfSize_,
		playerGroundColliderPosition_ + playerGroundColliderHalfSize_,
		});
}

void GameplayPhysicsDebugController::ApplyPlayerPhysicsCorrection(Player& player)
{
	// PhysicsWorldで補正された位置をPlayerへ戻し、壁へのめり込みを解消する。
	const K4E::Vector3 correctedColliderPosition = playerGroundCollider_.GetCenterPosition();
	K4E::Vector3 rawDelta = correctedColliderPosition - playerGroundColliderPosition_;
	const float maxCorrectionPerFrame = std::max(playerCorrectionClamp_, 0.0f);
	rawDelta.x = std::clamp(rawDelta.x, -maxCorrectionPerFrame, maxCorrectionPerFrame);
	rawDelta.y = std::clamp(rawDelta.y, -maxCorrectionPerFrame, maxCorrectionPerFrame);
	rawDelta.z = std::clamp(rawDelta.z, -maxCorrectionPerFrame, maxCorrectionPerFrame);

	K4E::Vector3 appliedDelta{};
	if (applyPlayerPhysicsCorrectionXZ_)
	{
		appliedDelta.x = rawDelta.x;
		appliedDelta.z = rawDelta.z;
	}
	if (applyPlayerPhysicsCorrectionY_)
	{
		appliedDelta.y = rawDelta.y;
	}

	playerPhysicsCorrectionDelta_ = appliedDelta;
	playerPositionAfterPhysics_ = playerPositionBeforePhysics_ + appliedDelta;
	if (K4E::Vector3::LengthSquared(appliedDelta) > 0.0f)
	{
		player.ApplyPhysicsCorrectedPosition(playerPositionAfterPhysics_);
		SyncPlayerPhysicsGroundCollider(player);
	}
}

bool GameplayPhysicsDebugController::EvaluatePlayerPhysicsGrounded()
{
	// PhysicsWorldのContact normalから、Player ColliderがStage上面に接しているかだけを評価する。
	playerStageContactCount_ = 0;
	bool grounded = false;
	constexpr float kGroundNormalThreshold = 0.5f;
	for (const K4E::Contact& contact : gameplayPhysicsWorld_.GetContacts())
	{
		const bool playerIsA = contact.colliderA == &playerGroundCollider_;
		const bool playerIsB = contact.colliderB == &playerGroundCollider_;
		if (!playerIsA && !playerIsB)
		{
			continue;
		}

		++playerStageContactCount_;
		if ((playerIsA && contact.normal.y < -kGroundNormalThreshold) ||
			(playerIsB && contact.normal.y > kGroundNormalThreshold))
		{
			grounded = true;
		}
	}

	return grounded;
}

void GameplayPhysicsDebugController::ApplyGameplayPhysicsParameterSettings()
{
	// JSON/ImGuiで調整した値をGameplay側PhysicsWorld/DebugDraw/テスト機能フラグへ反映する。
	gameplayPhysicsParameterBridge_.ApplyTo(gameplayPhysicsWorld_);
	gameplayPhysicsParameterBridge_.ApplyTo(gameplayPhysicsDebugDraw_);

	const K4E::GameplayPhysicsSettings settings = gameplayPhysicsParameterBridge_.GetGameplaySettings();
	const bool previousGameplayPhysicsTest = enableGameplayPhysicsTest_;
	const bool previousGroundCheck = enablePlayerPhysicsGroundCheck_;
	const bool previousDepenetration = enablePlayerPhysicsDepenetration_;
	const bool previousTriggerTest = enableGameplayPhysicsTriggerTest_;

	enableGameplayPhysicsTest_ = settings.enableGameplayPhysicsTest;
	enablePlayerPhysicsGroundCheck_ = settings.enablePlayerPhysicsGroundCheck;
	enablePlayerPhysicsDepenetration_ = settings.enablePlayerPhysicsDepenetration;
	applyPlayerPhysicsCorrectionXZ_ = settings.applyPlayerPhysicsCorrectionXZ;
	applyPlayerPhysicsCorrectionY_ = settings.applyPlayerPhysicsCorrectionY;
	playerCorrectionClamp_ = settings.playerCorrectionClamp;
	enableGameplayPhysicsTriggerTest_ = settings.enableGameplayPhysicsTriggerTest;
	enableGameplayPhysicsDebugDraw_ = gameplayPhysicsDebugDraw_.GetSettings().drawPhysicsDebug;
	usePhysicsForPlayerStage_ = settings.usePhysicsForPlayerStage || settings.enableGameplayPhysicsTest || settings.enablePlayerPhysicsGroundCheck || settings.enablePlayerPhysicsDepenetration;
	usePhysicsForPlayerGround_ = settings.usePhysicsForPlayerGround || settings.enablePlayerPhysicsGroundCheck;
	usePhysicsForPlayerDepenetration_ = settings.usePhysicsForPlayerDepenetration || settings.enablePlayerPhysicsDepenetration;
	usePhysicsForTriggerTest_ = settings.usePhysicsForTriggerTest || settings.enableGameplayPhysicsTriggerTest;
	usePhysicsForBulletTrigger_ = settings.usePhysicsForBulletTrigger;
	usePhysicsForEnemyStage_ = settings.usePhysicsForEnemyStage;
	if (deps_.bulletManager)
	{
		deps_.bulletManager->SetPhysicsTriggerWorld(&gameplayPhysicsWorld_, kPhysicsLayerPlayerBullet);
		deps_.bulletManager->SetUsePhysicsTriggerForNormalBullets(usePhysicsForBulletTrigger_);
	}
	UpdateCollisionSystemPolicyFromGameplayFlags();

	if (enableGameplayPhysicsTest_ && !previousGameplayPhysicsTest)
	{
		BindGameplayPhysicsStageColliders();
		ResetGameplayPhysicsTestObject();
	}
	if ((enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_) && (!previousGroundCheck && !previousDepenetration))
	{
		BindGameplayPhysicsStageColliders();
		RegisterPlayerPhysicsGroundCheck();
	}
	if (enableGameplayPhysicsTriggerTest_ != previousTriggerTest)
	{
		SetGameplayPhysicsTriggerTestEnabled(enableGameplayPhysicsTriggerTest_);
	}
	if (usePhysicsForBulletTrigger_)
	{
		BindGameplayPhysicsStageColliders();
		RegisterGameplayPhysicsEventListener();
	}
	else
	{
		UnregisterGameplayPhysicsBulletTriggerTargets();
	}
	if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_)
	{
		UnregisterPlayerPhysicsGroundCheck();
		if (!enableGameplayPhysicsTriggerTest_ && !usePhysicsForBulletTrigger_)
		{
			UnbindGameplayPhysicsStageColliders();
		}
	}
}

void GameplayPhysicsDebugController::UpdateCollisionSystemPolicyFromGameplayFlags()
{
	// 段階移行中に旧判定と新Physics判定を切り替えるため、現在のGameplay Physicsフラグを担当表へ反映する。
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PlayerStage,
		usePhysicsForPlayerStage_ || usePhysicsForPlayerGround_ || usePhysicsForPlayerDepenetration_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::TriggerTest,
		usePhysicsForTriggerTest_ || enableGameplayPhysicsTriggerTest_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::Disabled);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PhysicsTestObjectStage,
		enableGameplayPhysicsTest_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::Disabled);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PlayerBulletEnemy,
		usePhysicsForBulletTrigger_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PlayerBulletBoss,
		usePhysicsForBulletTrigger_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::EnemyStage,
		usePhysicsForEnemyStage_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
}

void GameplayPhysicsDebugController::DrawCollisionSystemPolicyImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Collision System Policy"))
	{
		return;
	}

	// Debug表示で、既存CollisionManagerとPhysicsWorldの責任範囲を一覧確認する。
	ImGui::Text("PhysicsWorld Collider Count: %zu", gameplayPhysicsWorld_.GetColliderCount());
	ImGui::Text("Legacy CollisionManager Collider Count: %zu", deps_.collisionManager ? deps_.collisionManager->GetColliderCount() : 0);
	ImGui::Text("Player vs Stage Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerStage)));
	ImGui::Text("Bullet vs Enemy Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerBulletEnemy)));
	ImGui::Text("BossAttack vs Player Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::BossAttackPlayer)));
	ImGui::Text("Trigger Test Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::TriggerTest)));

	if (collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerStage) == K4E::CollisionSystemOwner::PhysicsWorld)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Player vs Stage: existing movement remains active. Keep Physics correction flags explicit.");
	}
	if (collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerBulletEnemy) == K4E::CollisionSystemOwner::PhysicsWorld ||
		collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerBulletBoss) == K4E::CollisionSystemOwner::PhysicsWorld)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Bullet Trigger migration TODO: disable legacy damage before enabling real PhysicsWorld bullet hits.");
	}

	if (ImGui::TreeNode("Policy Table"))
	{
		for (const K4E::CollisionSystemRule& rule : collisionSystemPolicy_.GetRules())
		{
			ImGui::Text("%s | %s | %s",
				rule.pairName,
				K4E::CollisionSystemPolicy::ToString(rule.owner),
				rule.migrationStatus);
			if (rule.doubleProcessingRisk)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "Double-check");
			}
			ImGui::TextDisabled("%s", rule.note);
		}
		ImGui::TreePop();
	}
#endif
}

