#define NOMINMAX
#include "BossBattleController.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
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
#include "Stage.h"
#include <LogString.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
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
	ApplyCollisionPreset(*this, ECollisionPresetId::Item);
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
	if (!spawned_ || collected_) return;
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
	if (spawned_ && !collected_ && object3d_) object3d_->Draw();
}

bool BossClearItem::CheckPickup(const IPlayerRuntime& player) const
{
	if (!spawned_ || collected_) return false;
	const K4E::Vector3 diff = position_ - player.GetWorldPosition();
	return K4E::Vector3::Length(diff) <= pickupRadius_;
}

void BossClearItem::MarkCollected()
{
	collected_ = true;
	SetEnabled(false);
	SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
}

void BossClearItem::OnCollision(K4E::Collider* other) { (void)other; }

void BossBattleController::Initialize(GamePlayStageContext& stageContext, bool stage1BeginnerBalanceEnabled)
{
	stage1BeginnerBalanceEnabled_ = stage1BeginnerBalanceEnabled;
	bossSpawnPosition_ = stageContext.HasBossSpawnPoint() ? stageContext.GetBossSpawnPoint() : K4E::Vector3{ 0.0f, 2.25f, 30.0f };
	bossDeathPosition_ = bossSpawnPosition_;
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossSpawnConditionMet_ = false;
	bossDefeated_ = false;
	bossDeathPositionCaptured_ = false;
	clearItemSpawned_ = false;
	clearItemCollected_ = false;
	isGameClear_ = false;
	cameraShakeTimer_ = 0.0f;
	cameraShakeDuration_ = 0.0f;
	cameraShakeAmplitude_ = 0.0f;
	cameraShakeFrequency_ = 0.0f;
	cameraShakeSeed_ = 0.0f;
	lastPresentedBossPhase_ = BossPhase::Phase1;
	guardianBoss_.reset();
	clearItem_.reset();
	bossIntroController_.Initialize(bossSpawnPosition_);
}

void BossBattleController::Finalize(const Dependencies& deps)
{
	if (guardianBoss_ && deps.collisionManager) deps.collisionManager->RemoveCollider(guardianBoss_.get());
	if (clearItem_ && deps.collisionManager) deps.collisionManager->RemoveCollider(clearItem_.get());
	guardianBoss_.reset();
	clearItem_.reset();
	bossIntroController_.Finalize();
}

void BossBattleController::UpdateSpawnProgress(const Dependencies& deps)
{
	if (!deps.crystalManager) return;
	bossSpawnConditionMet_ = deps.crystalManager->AreAllCrystalsDestroyed() && !bossSpawned_;
	if (bossSpawnConditionMet_ && !bossIntroController_.HasPlayed() && !bossIntroController_.IsRunning()) bossIntroController_.RequestStart(bossSpawnPosition_);
}

void BossBattleController::UpdateIntro(const Dependencies& deps, float deltaTime)
{
	if (bossIntroController_.ConsumeDebugResetRequest()) ResetIntroForDebug(deps);
	if (bossIntroController_.ConsumeDebugStartRequest())
	{
		ResetIntroForDebug(deps);
		bossIntroController_.RequestStart(bossIntroController_.GetBossAppearPosition());
	}
	if (bossIntroController_.ConsumeDebugClearBossParentRequest() && guardianBoss_)
	{
		guardianBoss_->ClearRootParentKeepingWorldPosition();
		guardianBoss_->ForceSyncWorldTransform();
	}
	if (bossIntroController_.ConsumeDebugForceBossToAppearRequest() && guardianBoss_)
	{
		guardianBoss_->ClearRootParentKeepingWorldPosition();
		guardianBoss_->SetPosition(bossIntroController_.GetBossAppearPosition());
		guardianBoss_->SetYaw(kPi);
		guardianBoss_->ForceSyncWorldTransform();
	}
	if (bossIntroController_.ConsumeDebugUseGameplayViewProjectionRequest())
	{
		IPlayerRuntime* runtime = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (runtime && runtime->GetCamera())
		{
			K4E::CameraManager::GetInstance()->SetMainCamera(runtime->GetCamera());
			runtime->GetCamera()->Update();
		}
		if (guardianBoss_) guardianBoss_->ForceSyncWorldTransform();
	}

	if (!bossIntroController_.IsRunning()) return;
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (camera) K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	bossIntroController_.Update(deltaTime, guardianBoss_.get(), camera);

	if (bossIntroController_.ConsumeBossSpawnRequest())
	{
		bossSpawnPosition_ = bossIntroController_.GetBossAppearPosition();
		SpawnGuardianBoss(deps, false);
	}
	if (bossIntroController_.ConsumeBossColliderEnableRequest())
	{
		RegisterGuardianBossCollider(deps);
		if (player) AlignPlayerViewToBossAfterIntro(*player);
		if (deps.hudManager)
		{
			deps.hudManager->NotifyBossIntroCompleted(guardianBoss_ ? guardianBoss_->GetPosition() : bossSpawnPosition_);
			if (stage1BeginnerBalanceEnabled_) deps.hudManager->NotifyStage1BossAppeared();
		}
		StartCameraShake(0.75f, 0.42f, 24.0f);
	}
}

void BossBattleController::UpdateRuntime(const Dependencies& deps, float deltaTime)
{
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	if (guardianBoss_)
	{
		if (!bossDeathPositionCaptured_ && guardianBoss_->GetHP() <= 0.0f)
		{
			bossDeathPosition_ = guardianBoss_->GetPosition();
			bossDeathPositionCaptured_ = true; // 部位爆散でBodyが移動する前の撃破地点を報酬生成用に固定する。
		}
		if (player)
		{
			guardianBoss_->SetTargetPosition(player->GetWorldPosition());
			guardianBoss_->SetTargetPlayer(player);
		}
		guardianBoss_->Update(deltaTime);
		HandleBossPhasePresentation(deps);
	}
	UpdateCameraShake(deltaTime, player);
	UpdateBossClearProgress(deps, deltaTime);
	if (deps.crystalManager && deps.characters) deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
}

void BossBattleController::UpdatePausedWorld(const Dependencies& deps, float deltaTime)
{
	if (deps.crystalManager && deps.characters) deps.crystalManager->UpdatePresentationOnly(*deps.characters, deltaTime);
	UpdateIntro(deps, deltaTime);
	if (deps.stage) deps.stage->Update();
	UpdateBossClearProgress(deps, 0.0f);
	if (deps.crystalManager && deps.characters) deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
}

void BossBattleController::UpdateHud(const Dependencies& deps, float deltaTime)
{
	(void)deltaTime;
	if (deps.hudManager) deps.hudManager->SetBossHP(GetBossHP(), GetBossMaxHP(), IsBossBattleActive());
}

void BossBattleController::UpdateBossGuideHud(IPlayerRuntime& player, HUDManager& hudManager) const
{
	K4E::Camera* camera = player.GetCamera();
	if (!camera)
	{
		hudManager.SetBossGuide(player.GetWorldPosition(), bossSpawnPosition_, { 0.0f, 0.0f, 1.0f }, false);
		return;
	}
	const K4E::Vector3 bossPosition = guardianBoss_ ? guardianBoss_->GetPosition() : bossSpawnPosition_;
	hudManager.SetBossGuide(player.GetWorldPosition(), bossPosition, camera->GetForward(), IsBossBattleActive());
}

void BossBattleController::DrawBoss()
{
	if (guardianBoss_)
	{
		guardianBoss_->ForceSyncWorldTransform();
		guardianBoss_->Draw();
	}
}

void BossBattleController::DrawClearItem() { if (clearItem_) clearItem_->Draw(); }
void BossBattleController::DrawBossIntro3D() { if (guardianBoss_) { guardianBoss_->ForceSyncWorldTransform(); guardianBoss_->Draw(); } }
void BossBattleController::DrawShadow() { if (guardianBoss_) guardianBoss_->DrawShadow(); }
void BossBattleController::DrawBossIntroShadow() { if (guardianBoss_) guardianBoss_->DrawShadow(); }

void BossBattleController::DrawImGui(const Dependencies& deps, bool bossIntroPresentationActive)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("ボス状態");
	ImGui::Text("ボス出現済み: %s", bossSpawned_ ? "はい" : "いいえ");
	ImGui::Text("ボスCollider登録済み: %s", bossColliderRegistered_ ? "はい" : "いいえ");
	ImGui::Text("ボス登場演出中: %s", bossIntroController_.IsRunning() ? "はい" : "いいえ");
	ImGui::Text("ボス登場による進行停止: %s", bossIntroController_.IsGameplayPaused() ? "はい" : "いいえ");
	ImGui::Text("ボス演出カメラ揺れ: %.2f / %.2f amp %.2f freq %.2f", cameraShakeTimer_, cameraShakeDuration_, cameraShakeAmplitude_, cameraShakeFrequency_);
	IPlayerRuntime* runtime = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	bossIntroController_.SetDebugSnapshot(guardianBoss_.get(), runtime ? runtime->GetCamera() : nullptr);
	bossIntroController_.DrawImGui();
	ImGui::SeparatorText("Boss Intro Draw Debug");
	ImGui::Text("Camera Kind: %s", bossIntroPresentationActive ? "BossIntro Camera" : "Gameplay Camera");
	if (guardianBoss_)
	{
		ImGui::Text("ボス生存中: %s", guardianBoss_->IsAlive() ? "はい" : "いいえ");
		ImGui::Text("ボスHP: %.1f / %.1f", guardianBoss_->GetHP(), guardianBoss_->GetMaxHP());
		ImGui::Text("ボス攻撃ヒット回数: %d", guardianBoss_->GetBossAttackHitCount());
		ImGui::Text("最後にプレイヤーが受けたボスダメージ: %.1f", guardianBoss_->GetLastPlayerDamage());
	}
	ImGui::SeparatorText("クリアCube状態");
	ImGui::Text("死亡地点保持: %s (%.2f, %.2f, %.2f)", bossDeathPositionCaptured_ ? "はい" : "いいえ", bossDeathPosition_.x, bossDeathPosition_.y, bossDeathPosition_.z);
	ImGui::Text("クリアCube出現済み: %s", clearItemSpawned_ ? "はい" : "いいえ");
	ImGui::Text("クリアCube取得済み: %s", clearItemCollected_ ? "はい" : "いいえ");
	ImGui::Text("ゲームクリア判定: %s", isGameClear_ ? "はい" : "いいえ");
	if (guardianBoss_) guardianBoss_->DrawImGui();
#else
	(void)deps;
	(void)bossIntroPresentationActive;
#endif
}

void BossBattleController::ResetIntroForDebug(const Dependencies& deps)
{
	if (guardianBoss_ && deps.collisionManager && bossColliderRegistered_) deps.collisionManager->RemoveCollider(guardianBoss_.get());
	guardianBoss_.reset();
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossDefeated_ = false;
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	bossSpawnConditionMet_ = false;
	bossIntroController_.Reset();
}

bool BossBattleController::IsBossBattleActive() const { return guardianBoss_ && bossColliderRegistered_ && guardianBoss_->IsAlive() && !bossIntroController_.IsGameplayPaused(); }
float BossBattleController::GetBossHP() const { return guardianBoss_ ? guardianBoss_->GetHP() : 0.0f; }
float BossBattleController::GetBossMaxHP() const { return guardianBoss_ ? guardianBoss_->GetMaxHP() : 0.0f; }

void BossBattleController::SpawnGuardianBoss(const Dependencies& deps, bool registerCollider)
{
	if (bossSpawned_) return;
	guardianBoss_ = std::make_unique<GuardianBoss>();
	guardianBoss_->Initialize();
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	if (stage1BeginnerBalanceEnabled_)
	{
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
		bossCollisionSettings.eps = 0.002f;
		guardianBoss_->SetWorldCollisionSettings(bossCollisionSettings);
	}
	guardianBoss_->SetPosition(registerCollider ? bossSpawnPosition_ : bossIntroController_.GetBossStartPosition());
	guardianBoss_->SetYaw(kPi);
	if (deps.characters && deps.characters->GetPlayerRuntime())
	{
		IPlayerRuntime* player = deps.characters->GetPlayerRuntime();
		guardianBoss_->SetTargetPosition(player->GetWorldPosition());
		guardianBoss_->SetTargetPlayer(player);
	}
	guardianBoss_->Update(0.0f);
	bossSpawned_ = true;
	if (registerCollider) RegisterGuardianBossCollider(deps);
}

void BossBattleController::RegisterGuardianBossCollider(const Dependencies& deps)
{
	if (!guardianBoss_ || !deps.collisionManager || bossColliderRegistered_) return;
	guardianBoss_->ClearRootParentKeepingWorldPosition();
	guardianBoss_->SetPosition(bossIntroController_.GetBossAppearPosition());
	guardianBoss_->SetYaw(kPi);
	guardianBoss_->ForceSyncWorldTransform();
	deps.collisionManager->AddCollider(guardianBoss_.get());
	bossColliderRegistered_ = true;
	Log("[GuardianBoss] Collider registered as kBoss.\n");
}

void BossBattleController::AlignPlayerViewToBossAfterIntro(IPlayerRuntime& player) const
{
	K4E::Camera* resumedCamera = player.GetCamera();
	if (!resumedCamera) return;
	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTarget(resumedCamera->GetTranslate(), bossIntroController_.GetBossLookTarget(), pitch, yaw)) player.SetViewLookAngles(pitch, yaw);
	K4E::CameraManager::GetInstance()->SetMainCamera(resumedCamera);
	resumedCamera->Update();
}

void BossBattleController::UpdateBossClearProgress(const Dependencies& deps, float deltaTime)
{
	if (guardianBoss_ && guardianBoss_->IsDead() && !bossDefeated_)
	{
		bossDefeated_ = true;
		if (deps.setBossDefeated) deps.setBossDefeated(false);
	}
	if (bossDefeated_ && !clearItemSpawned_ && bossDeathPositionCaptured_) SpawnClearItem(deps, bossDeathPosition_);
	if (clearItem_ && !clearItemCollected_)
	{
		clearItem_->Update(deltaTime);
		IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (player && clearItem_->CheckPickup(*player)) CollectClearItem(deps);
	}
}

void BossBattleController::SpawnClearItem(const Dependencies& deps, const K4E::Vector3& bossPosition)
{
	if (clearItemSpawned_) return;
	K4E::Vector3 spawnPosition = bossPosition;
	if (deps.stage)
	{
		float selectedFloorY = -std::numeric_limits<float>::infinity();
		float nearestHeight = std::numeric_limits<float>::infinity();
		for (const K4E::AABB& floor : deps.stage->GetFloorAABBs())
		{
			if (spawnPosition.x < floor.min.x || spawnPosition.x > floor.max.x || spawnPosition.z < floor.min.z || spawnPosition.z > floor.max.z) continue;
			const float heightDistance = std::abs(floor.max.y - spawnPosition.y);
			if (heightDistance < nearestHeight)
			{
				nearestHeight = heightDistance;
				selectedFloorY = floor.max.y;
			}
		}
		if (std::isfinite(selectedFloorY)) spawnPosition.y = selectedFloorY;
	}
	spawnPosition.y = std::max(spawnPosition.y, 0.0f);
	clearItem_ = std::make_unique<BossClearItem>();
	clearItem_->Initialize(spawnPosition); // XZは撃破地点を維持し、Yだけ最寄り床面へ合わせる。
	if (deps.collisionManager) deps.collisionManager->AddCollider(clearItem_.get());
	clearItemSpawned_ = true;
	Log("[GameClear] BossClearItem spawned at captured death position.\n");
}

void BossBattleController::CollectClearItem(const Dependencies& deps)
{
	if (clearItemCollected_ || isGameClear_) return;
	clearItemCollected_ = true;
	isGameClear_ = true;
	if (clearItem_)
	{
		clearItem_->MarkCollected();
		if (deps.collisionManager) deps.collisionManager->RemoveCollider(clearItem_.get());
	}
	if (deps.setBossDefeated) deps.setBossDefeated(true);
	Log("[GameClear] Clear item collected.\n");
}

void BossBattleController::HandleBossPhasePresentation(const Dependencies& deps)
{
	(void)deps;
	if (!guardianBoss_) return;
	BossPhase phase = BossPhase::Phase1;
	if (!guardianBoss_->ConsumePhaseTransitionPresentation(phase)) return;
	lastPresentedBossPhase_ = phase;
	if (phase == BossPhase::Phase3) StartCameraShake(0.85f, 0.40f, 25.0f);
	else if (phase == BossPhase::Phase2) StartCameraShake(0.55f, 0.24f, 20.0f);
}

void BossBattleController::StartCameraShake(float duration, float amplitude, float frequency)
{
	if (duration <= 0.0f || amplitude <= 0.0f) return;
	cameraShakeDuration_ = duration;
	cameraShakeTimer_ = duration;
	cameraShakeAmplitude_ = amplitude;
	cameraShakeFrequency_ = std::max(1.0f, frequency);
	cameraShakeSeed_ += 2.31f;
}

void BossBattleController::UpdateCameraShake(float deltaTime, IPlayerRuntime* player)
{
	if (cameraShakeTimer_ <= 0.0f) return;
	cameraShakeTimer_ = std::max(0.0f, cameraShakeTimer_ - deltaTime);
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (!camera) return;
	camera->SetTranslate(camera->GetTranslate() + BuildCameraShakeOffset());
	camera->Update();
}

K4E::Vector3 BossBattleController::BuildCameraShakeOffset() const
{
	if (cameraShakeTimer_ <= 0.0f || cameraShakeDuration_ <= 0.0f) return {};
	const float remainRate = std::clamp(cameraShakeTimer_ / cameraShakeDuration_, 0.0f, 1.0f);
	const float t = (cameraShakeDuration_ - cameraShakeTimer_) * cameraShakeFrequency_ + cameraShakeSeed_;
	const float amp = cameraShakeAmplitude_ * remainRate * remainRate;
	return { std::sin(t * 1.51f) * amp, std::cos(t * 1.13f) * amp * 0.50f, std::sin(t * 0.83f) * amp * 0.30f };
}

bool BossBattleController::CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& outPitch, float& outYaw)
{
	K4E::Vector3 direction = target - from;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f) return false;
	direction = K4E::Vector3::Normalize(direction);
	outYaw = std::atan2(-direction.x, direction.z);
	const float xzLen = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	outPitch = std::atan2(-direction.y, xzLen);
	return true;
}