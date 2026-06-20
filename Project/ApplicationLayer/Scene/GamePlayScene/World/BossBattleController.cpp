#define NOMINMAX
#include "BossBattleController.h"

#include "Camera.h"
#include "CameraManager.h"
#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "CrystalManager.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "GamePlayStageContext.h"
#include "HUDManager.h"
#include "Player.h"
#include "Stage.h"
#include <LogString.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;
	constexpr float kStage1BeginnerBossMaxHP = 900.0f;
}

void BossClearItem::Initialize(const K4E::Vector3& position)
{
	K4E::Vector3 spawnPosition = position;
	spawnPosition.y += 1.25f;

	position_ = spawnPosition;
	basePosition_ = spawnPosition;
	rotation_ = {};
	floatTimer_ = 0.0f;
	spawned_ = true;
	collected_ = false;

	ApplyCollisionPreset(*this, ECollisionPresetId::Item); // BossClearItemは通常Itemと同じkItem判定を保つPreset移行対象にする。
#ifdef _DEBUG
	const uint32_t legacyItemTypeId = static_cast<uint32_t>(CollisionTypeIdDef::kItem);
	assert(GetTypeID() == legacyItemTypeId && "BossClearItem preset must keep legacy kItem TypeID.");
#endif
	SetOwner<BossClearItem>(this);
	SetOBBHalfSize(halfSize_);
	SetCenterPosition(position_);

	object3d_ = std::make_unique<K4E::Object3D>();
	object3d_->Initialize("Sample/cube.gltf");
	object3d_->SetScale({ 1.8f, 1.8f, 1.8f });
	object3d_->SetTranslate(position_);
	object3d_->SetColor({ 1.0f, 0.85f, 0.05f, 1.0f });
	object3d_->Update();
}

void BossClearItem::Update(float deltaTime)
{
	if (!spawned_ || collected_)
	{
		return;
	}

	floatTimer_ += deltaTime * 3.0f;
	position_ = basePosition_;
	position_.y += std::sinf(floatTimer_) * 0.25f;
	rotation_.y += deltaTime * 1.2f;

	SetCenterPosition(position_);
	SetOrientation(rotation_);

	if (object3d_)
	{
		object3d_->SetTranslate(position_);
		object3d_->SetRotate(rotation_);
		object3d_->Update();
	}
}

void BossClearItem::Draw()
{
	if (spawned_ && !collected_ && object3d_)
	{
		object3d_->Draw();
	}
}

bool BossClearItem::CheckPickup(const Player& player) const
{
	if (!spawned_ || collected_)
	{
		return false;
	}

	const K4E::Vector3 diff = position_ - player.GetCenterPosition();
	return K4E::Vector3::Length(diff) <= pickupRadius_;
}

void BossClearItem::MarkCollected()
{
	collected_ = true;
	SetEnabled(false);
	SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
}

void BossClearItem::OnCollision(K4E::Collider* other)
{
	(void)other;
}

void BossBattleController::Initialize(GamePlayStageContext& stageContext, bool stage1BeginnerBalanceEnabled)
{
	stage1BeginnerBalanceEnabled_ = stage1BeginnerBalanceEnabled;
	bossSpawnPosition_ = stageContext.HasBossSpawnPoint() ? stageContext.GetBossSpawnPoint() : K4E::Vector3{ 0.0f, 2.25f, 30.0f };
	if (!stageContext.HasBossSpawnPoint())
	{
		// Blender側BossSpawnPointが未設定の間は、DebugSceneと同じ仮固定座標を使う。
		bossSpawnPosition_ = { 0.0f, 2.25f, 30.0f };
	}

	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossSpawnConditionMet_ = false;
	bossDefeated_ = false;
	clearItemSpawned_ = false;
	clearItemCollected_ = false;
	isGameClear_ = false;
	guardianBoss_.reset();
	clearItem_.reset();
	bossIntroController_.Initialize(bossSpawnPosition_);
}

void BossBattleController::Finalize(const Dependencies& deps)
{
	if (guardianBoss_ && deps.collisionManager)
	{
		deps.collisionManager->RemoveCollider(guardianBoss_.get());
	}
	if (clearItem_ && deps.collisionManager)
	{
		deps.collisionManager->RemoveCollider(clearItem_.get());
	}
	guardianBoss_.reset();
	clearItem_.reset();
	bossIntroController_.Finalize();
}

void BossBattleController::UpdateSpawnProgress(const Dependencies& deps)
{
	if (!deps.crystalManager)
	{
		return;
	}

	bossSpawnConditionMet_ = deps.crystalManager->AreAllCrystalsDestroyed() && !bossSpawned_;

	// 全クリスタル破壊を検知し、即スポーンではなくボス登場遅延を開始する。
	if (bossSpawnConditionMet_ && !bossIntroController_.HasPlayed() && !bossIntroController_.IsRunning())
	{
		bossIntroController_.RequestStart(bossSpawnPosition_);
	}
}

void BossBattleController::UpdateIntro(const Dependencies& deps, float deltaTime)
{
	if (bossIntroController_.ConsumeDebugResetRequest())
	{
		ResetIntroForDebug(deps);
	}

	if (bossIntroController_.ConsumeDebugStartRequest())
	{
		ResetIntroForDebug(deps);
		bossIntroController_.RequestStart(bossIntroController_.GetBossAppearPosition());
	}

	if (bossIntroController_.ConsumeDebugClearBossParentRequest() && guardianBoss_)
	{
		// 検証用: 親子Transformが原因か切り分けるため、ワールド座標を維持して親を外す。
		guardianBoss_->ClearRootParentKeepingWorldPosition();
		guardianBoss_->ForceSyncWorldTransform();
	}

	if (bossIntroController_.ConsumeDebugForceBossToAppearRequest() && guardianBoss_)
	{
		guardianBoss_->ClearRootParentKeepingWorldPosition();
		// 検証用: ボスを最終ワールド座標へ固定して、カメラ追従に見える原因がTransformか確認する。
		guardianBoss_->SetPosition(bossIntroController_.GetBossAppearPosition());
		guardianBoss_->SetYaw(kPi);
		guardianBoss_->ForceSyncWorldTransform();
	}

	if (bossIntroController_.ConsumeDebugUseGameplayViewProjectionRequest())
	{
		if (deps.characters)
		{
			if (auto* debugPlayer = deps.characters->GetPlayer())
			{
				if (auto* gameplayCamera = debugPlayer->GetCamera())
				{
					// 検証用: 演出用ViewProjectionから通常ViewProjectionへ戻して、描画行列側の問題を切り分ける。
					K4E::CameraManager::GetInstance()->SetMainCamera(gameplayCamera);
					gameplayCamera->Update();
				}
			}
		}
		if (guardianBoss_)
		{
			guardianBoss_->ForceSyncWorldTransform();
		}
	}

	if (!bossIntroController_.IsRunning())
	{
		return;
	}

	Player* player = deps.characters ? deps.characters->GetPlayer() : nullptr;
	auto* camera = player ? player->GetCamera() : nullptr;
	if (camera)
	{
		K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	}
	bossIntroController_.Update(deltaTime, guardianBoss_.get(), camera);

	if (bossIntroController_.ConsumeBossSpawnRequest())
	{
		bossSpawnPosition_ = bossIntroController_.GetBossAppearPosition();
		SpawnGuardianBoss(deps, false);
	}

	if (bossIntroController_.ConsumeBossColliderEnableRequest())
	{
		RegisterGuardianBossCollider(deps);
		if (player)
		{
			AlignPlayerViewToBossAfterIntro(*player);
		}
		if (deps.hudManager)
		{
			deps.hudManager->NotifyBossIntroCompleted(guardianBoss_ ? guardianBoss_->GetPosition() : bossSpawnPosition_);
			if (stage1BeginnerBalanceEnabled_)
			{
				deps.hudManager->NotifyStage1BossAppeared();
			}
		}
	}
}

void BossBattleController::UpdateRuntime(const Dependencies& deps, float deltaTime)
{
	if (guardianBoss_)
	{
		if (deps.characters)
		{
			if (auto* player = deps.characters->GetPlayer())
			{
				guardianBoss_->SetTargetPosition(player->GetCenterPosition());
				guardianBoss_->SetTargetPlayer(player);
			}
		}
		guardianBoss_->Update(deltaTime);
	}

	UpdateBossClearProgress(deps, deltaTime);
	if (deps.crystalManager && deps.characters)
	{
		deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
	}
}

void BossBattleController::UpdatePausedWorld(const Dependencies& deps, float deltaTime)
{
	if (deps.crystalManager && deps.characters)
	{
		deps.crystalManager->UpdatePresentationOnly(*deps.characters, deltaTime);
	}
	UpdateIntro(deps, deltaTime);
	if (deps.stage)
	{
		// 演出用カメラと通常カメラを切り替えた後、最低限表示するステージのWVPを現在カメラへ合わせる。
		deps.stage->Update();
	}
	UpdateBossClearProgress(deps, 0.0f);
	if (deps.crystalManager && deps.characters)
	{
		deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
	}
}

void BossBattleController::UpdateHud(const Dependencies& deps, float deltaTime)
{
	(void)deltaTime;
	if (!deps.hudManager)
	{
		return;
	}

	deps.hudManager->SetBossHP(GetBossHP(), GetBossMaxHP(), IsBossBattleActive());
}

void BossBattleController::UpdateBossGuideHud(Player& player, HUDManager& hudManager) const
{
	auto* camera = player.GetCamera();
	if (!camera)
	{
		hudManager.SetBossGuide(player.GetCenterPosition(), bossSpawnPosition_, { 0.0f, 0.0f, 1.0f }, false);
		return;
	}

	const K4E::Vector3 bossPosition = guardianBoss_ ? guardianBoss_->GetPosition() : bossSpawnPosition_;
	hudManager.SetBossGuide(player.GetCenterPosition(), bossPosition, camera->GetForward(), IsBossBattleActive());
}

void BossBattleController::DrawBoss()
{
	if (guardianBoss_)
	{
		// Draw直前に現在の通常ViewProjectionでWVPを更新し、演出用ViewProjectionの残留を防ぐ。
		guardianBoss_->ForceSyncWorldTransform();
		guardianBoss_->Draw();
	}
}

void BossBattleController::DrawClearItem()
{
	if (clearItem_)
	{
		clearItem_->Draw();
	}
}

void BossBattleController::DrawBossIntro3D()
{
	if (guardianBoss_)
	{
		// ボス登場演出中に通常3D描画を止め、ボスだけを現在の演出用ViewProjectionへ同期する。
		guardianBoss_->ForceSyncWorldTransform();
		guardianBoss_->Draw();
	}
}

void BossBattleController::DrawShadow()
{
	if (guardianBoss_)
	{
		guardianBoss_->DrawShadow();
	}
}

void BossBattleController::DrawBossIntroShadow()
{
	if (guardianBoss_)
	{
		guardianBoss_->DrawShadow();
	}
}

void BossBattleController::DrawImGui(const Dependencies& deps, bool bossIntroPresentationActive)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("ボス状態");
	ImGui::Text("ボス出現済み: %s", bossSpawned_ ? "はい" : "いいえ");
	ImGui::Text("ボスCollider登録済み: %s", bossColliderRegistered_ ? "はい" : "いいえ");
	ImGui::Text("ボス登場演出中: %s", bossIntroController_.IsRunning() ? "はい" : "いいえ");
	ImGui::Text("ボス登場による進行停止: %s", bossIntroController_.IsGameplayPaused() ? "はい" : "いいえ");
	{
		auto* debugPlayer = deps.characters ? deps.characters->GetPlayer() : nullptr;
		bossIntroController_.SetDebugSnapshot(guardianBoss_.get(), debugPlayer ? debugPlayer->GetCamera() : nullptr);
	}
	bossIntroController_.DrawImGui();
	ImGui::SeparatorText("Boss Intro Draw Debug");
	ImGui::Text("isBossIntroActive: %s", bossIntroController_.IsRunning() ? "true" : "false");
	ImGui::Text("Camera Kind: %s", bossIntroPresentationActive ? "BossIntro Camera" : "Gameplay Camera");
	ImGui::Text("ViewProjection Kind: %s", bossIntroPresentationActive ? "BossIntro ViewProjection" : "Gameplay ViewProjection");
	ImGui::Text("Draw Gameplay 3D: %s", bossIntroPresentationActive ? "false" : "true");
	ImGui::Text("Draw Gameplay UI: %s", bossIntroPresentationActive ? "false" : "true");
	ImGui::Text("Draw BossIntro 3D: %s", bossIntroPresentationActive ? "true" : "false");
	ImGui::Text("Draw Gameplay Route: %s", bossIntroPresentationActive ? "false" : "true");
	if (guardianBoss_)
	{
		ImGui::Text("ボス生存中: %s", guardianBoss_->IsAlive() ? "はい" : "いいえ");
		ImGui::Text("ボスHP: %.1f", guardianBoss_->GetHP());
		ImGui::Text("ボス最大HP: %.1f", guardianBoss_->GetMaxHP());
		ImGui::Text("ボスHP割合: %.1f%%", guardianBoss_->GetHPRate() * 100.0f);
		ImGui::Text("近接攻撃ヒット回数: %d", guardianBoss_->GetMeleeHitCount());
		ImGui::Text("銃弾ヒット回数: %d", guardianBoss_->GetBulletHitCount());
		ImGui::Text("最後にボスへ与えたダメージ: %.1f", guardianBoss_->GetLastReceivedDamage());
		ImGui::Text("ボス攻撃ヒット回数: %d", guardianBoss_->GetBossAttackHitCount());
		ImGui::Text("最後にプレイヤーが受けたボスダメージ: %.1f", guardianBoss_->GetLastPlayerDamage());
	}
	else
	{
		ImGui::Text("ボス生存中: いいえ");
		ImGui::Text("ボスHP: 0.0");
		ImGui::Text("ボス最大HP: 0.0");
		ImGui::Text("ボスHP割合: 0.0%%");
		ImGui::Text("近接攻撃ヒット回数: 0");
		ImGui::Text("銃弾ヒット回数: 0");
		ImGui::Text("最後にボスへ与えたダメージ: 0.0");
		ImGui::Text("ボス攻撃ヒット回数: 0");
		ImGui::Text("最後にプレイヤーが受けたボスダメージ: 0.0");
	}

	ImGui::SeparatorText("クリアCube状態");
	ImGui::Text("クリアCube出現済み: %s", clearItemSpawned_ ? "はい" : "いいえ");
	ImGui::Text("クリアCube取得済み: %s", clearItemCollected_ ? "はい" : "いいえ");
	const K4E::Vector3 clearPos = clearItem_ ? clearItem_->GetPosition() : K4E::Vector3{};
	ImGui::Text("クリアCube座標: %.2f, %.2f, %.2f", clearPos.x, clearPos.y, clearPos.z);
	ImGui::Text("ゲームクリア判定: %s", isGameClear_ ? "はい" : "いいえ");
	ImGui::Text("ボス撃破済み: %s", bossDefeated_ ? "はい" : "いいえ");
	if (guardianBoss_)
	{
		guardianBoss_->DrawImGui();
	}
#else
	(void)deps;
	(void)bossIntroPresentationActive;
#endif
}

void BossBattleController::ResetIntroForDebug(const Dependencies& deps)
{
	if (guardianBoss_ && deps.collisionManager && bossColliderRegistered_)
	{
		deps.collisionManager->RemoveCollider(guardianBoss_.get());
	}

	guardianBoss_.reset();
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossDefeated_ = false;
	bossSpawnConditionMet_ = false;
	bossIntroController_.Reset();
}

bool BossBattleController::IsBossBattleActive() const
{
	return guardianBoss_ && bossColliderRegistered_ && guardianBoss_->IsAlive() && !bossIntroController_.IsGameplayPaused();
}

float BossBattleController::GetBossHP() const
{
	return guardianBoss_ ? guardianBoss_->GetHP() : 0.0f;
}

float BossBattleController::GetBossMaxHP() const
{
	return guardianBoss_ ? guardianBoss_->GetMaxHP() : 0.0f;
}

void BossBattleController::SpawnGuardianBoss(const Dependencies& deps, bool registerCollider)
{
	if (bossSpawned_)
	{
		return;
	}

	guardianBoss_ = std::make_unique<GuardianBoss>();
	guardianBoss_->Initialize();
	if (stage1BeginnerBalanceEnabled_)
	{
		// ステージ1はプライマリ武器1丁で倒し切れるよう、ボスHPだけを導入ステージ用に下げる。
		if (auto* status = guardianBoss_->GetStatusComponent())
		{
			status->SetMaxHP(kStage1BeginnerBossMaxHP);
			status->SetHP(kStage1BeginnerBossMaxHP);
		}
	}

	if (deps.stage)
	{
		guardianBoss_->SetStageObstacleAABBs(&deps.stage->GetWallObstacleAABBs());

		K4E::WorldCollisionSettings bossCollisionSettings{};
		bossCollisionSettings.half = { 1.25f, 1.75f, 1.25f };
		bossCollisionSettings.centerOffset = { 0.0f, 0.0f, 0.0f };
		bossCollisionSettings.eps = 0.002f; // ボス本体のColliderサイズに合わせて、障害物との押し戻しサイズを設定する。
		guardianBoss_->SetWorldCollisionSettings(bossCollisionSettings);
	}

	guardianBoss_->SetPosition(registerCollider ? bossSpawnPosition_ : bossIntroController_.GetBossStartPosition());
	guardianBoss_->SetYaw(kPi);
	if (deps.characters)
	{
		if (auto* player = deps.characters->GetPlayer())
		{
			guardianBoss_->SetTargetPosition(player->GetCenterPosition());
			guardianBoss_->SetTargetPlayer(player);
		}
	}
	guardianBoss_->Update(0.0f);
	bossSpawned_ = true;
	if (registerCollider)
	{
		RegisterGuardianBossCollider(deps);
	}
}

void BossBattleController::RegisterGuardianBossCollider(const Dependencies& deps)
{
	if (!guardianBoss_ || !deps.collisionManager || bossColliderRegistered_)
	{
		return;
	}

	guardianBoss_->ClearRootParentKeepingWorldPosition();
	guardianBoss_->SetPosition(bossIntroController_.GetBossAppearPosition());
	guardianBoss_->SetYaw(kPi);
	guardianBoss_->ForceSyncWorldTransform();
	// 登場完了後にボスAI/攻撃/当たり判定を有効化するため、このタイミングでCollider登録する。
	deps.collisionManager->AddCollider(guardianBoss_.get());
	bossColliderRegistered_ = true;
	Log("[GuardianBoss] Collider registered as kBoss.\n");
}

void BossBattleController::AlignPlayerViewToBossAfterIntro(Player& player) const
{
	auto* resumedCamera = player.GetCamera();
	if (!resumedCamera)
	{
		return;
	}

	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTarget(resumedCamera->GetTranslate(), bossIntroController_.GetBossLookTarget(), pitch, yaw))
	{
		// カットシーン復帰時も通常FPSカメラの内部角度をボス方向へ合わせ、次フレームで元視線へ戻らないようにする。
		player.SetViewLookAngles(pitch, yaw);
	}

	player.SyncViewToPlayer();
	K4E::CameraManager::GetInstance()->SetMainCamera(resumedCamera);
	resumedCamera->Update();
}

void BossBattleController::UpdateBossClearProgress(const Dependencies& deps, float deltaTime)
{
	if (guardianBoss_ && guardianBoss_->IsDead() && !bossDefeated_)
	{
		// ボス死亡だけでは即クリアにせず、取得アイテムを出すための中間状態にする。
		bossDefeated_ = true;
		if (deps.setBossDefeated)
		{
			deps.setBossDefeated(false);
		}
	}

	if (bossDefeated_ && !clearItemSpawned_ && guardianBoss_)
	{
		// 撃破位置を基準にクリアアイテムを出し、プレイヤーが取りに行く余地を残す。
		SpawnClearItem(deps, guardianBoss_->GetPosition());
	}

	if (clearItem_ && !clearItemCollected_)
	{
		clearItem_->Update(deltaTime);
		if (deps.characters)
		{
			if (auto* player = deps.characters->GetPlayer())
			{
				if (clearItem_->CheckPickup(*player))
				{
					CollectClearItem(deps);
				}
			}
		}
	}
}

void BossBattleController::SpawnClearItem(const Dependencies& deps, const K4E::Vector3& bossPosition)
{
	if (clearItemSpawned_)
	{
		return;
	}

	K4E::Vector3 spawnPosition = bossPosition;
	spawnPosition.z -= 2.0f;
	spawnPosition.y = std::max(spawnPosition.y, 0.75f);

	clearItem_ = std::make_unique<BossClearItem>();
	clearItem_->Initialize(spawnPosition);
	if (deps.collisionManager)
	{
		deps.collisionManager->AddCollider(clearItem_.get());
	}

	clearItemSpawned_ = true;
	Log("[GameClear] BossClearItem spawned.\n");
}

void BossBattleController::CollectClearItem(const Dependencies& deps)
{
	if (clearItemCollected_ || isGameClear_)
	{
		return;
	}

	clearItemCollected_ = true;
	isGameClear_ = true;
	if (clearItem_)
	{
		clearItem_->MarkCollected();
		if (deps.collisionManager)
		{
			deps.collisionManager->RemoveCollider(clearItem_.get());
		}
	}

	// ボス撃破後に出現するクリアCubeを取得したらゲームクリアへ進める。
	if (deps.setBossDefeated)
	{
		deps.setBossDefeated(true);
	}
	Log("[GameClear] Clear item collected.\n");
}

bool BossBattleController::CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& outPitch, float& outYaw)
{
	K4E::Vector3 direction = target - from;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f)
	{
		return false;
	}

	direction = K4E::Vector3::Normalize(direction);
	outYaw = std::atan2(-direction.x, direction.z);
	const float xzLen = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	outPitch = std::atan2(-direction.y, xzLen);
	return true;
}
