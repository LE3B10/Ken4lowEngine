#define NOMINMAX
#include "BossBattleController.h"

#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/DebugScene/DebugActorRegistration.h"
#include "Camera.h"
#include "CameraManager.h"
#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "CrystalManager.h"
#include "GamePlayStageContext.h"
#include "HUDManager.h"
#include "Scene/Actor/Character/CharacterHealthComponent.h"
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
	constexpr float kTwoPi = kPi * 2.0f;
	constexpr float kBeginnerBossMaxHp = 900.0f;
	constexpr int kForgottenMineStageIndex = 1;
	constexpr const char* kBossPrefabPath = "Resources/ActorPrefabs/ComponentBoss.json";
	constexpr const char* kMineBlockModelPath = "Sample/cube.gltf";

	float Approach(float current, float target, float speed, float deltaTime)
	{
		const float step = std::max(0.0f, speed * deltaTime);
		if (current < target) return std::min(target, current + step);
		return std::max(target, current - step);
	}
}

void BossClearItem::Initialize(const K4E::Vector3& position)
{
	position_ = position + K4E::Vector3{ 0.0f, 1.25f, 0.0f };
	basePosition_ = position_;
	rotation_ = {};
	floatTimer_ = 0.0f;
	spawned_ = true;
	collected_ = false;
	ApplyCollisionPreset(*this, ECollisionPresetId::Item);
#ifdef _DEBUG
	assert(GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kItem));
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
	position_.y += std::sin(floatTimer_) * 0.25f;
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

void BossClearItem::Draw(){ if (spawned_ && !collected_ && object3d_) object3d_->Draw(); }

bool BossClearItem::CheckPickup(const IPlayerRuntime& player) const
{
	return spawned_ && !collected_ && K4E::Vector3::Length(position_ - player.GetWorldPosition()) <= pickupRadius_;
}

void BossClearItem::MarkCollected()
{
	collected_ = true;
	SetEnabled(false);
	SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
}

void BossClearItem::OnCollision(K4E::Collider* other){ (void)other; }

void BossBattleController::Initialize(GamePlayStageContext& stageContext, bool beginnerBalance)
{
	stage1BeginnerBalanceEnabled_ = beginnerBalance;
	mineArenaEnabled_ = stageContext.GetCurrentStageIndex() == kForgottenMineStageIndex;
	mineArenaInitialized_ = false;
	minePassageUnlocked_ = false;
	mineArenaEntered_ = false;
	minePassageDoorOpenAmount_ = 0.0f;
	mineArenaGateOpenAmount_ = 1.0f;
	mineArenaBlocks_.clear();
	mineCorridorLightIndices_.clear();
	mineArenaLightIndices_.clear();
	minePassageDoorLeftIndex_ = minePassageDoorRightIndex_ = kInvalidMineBlockIndex;
	mineArenaGateLeftIndex_ = mineArenaGateRightIndex_ = kInvalidMineBlockIndex;
	mineLightingSaved_ = false;

	bossSpawnPosition_ = mineArenaEnabled_
		? mineArenaCenter_
		: (stageContext.HasBossSpawnPoint() ? stageContext.GetBossSpawnPoint() : K4E::Vector3{ 0.0f, 2.25f, 30.0f });
	bossDeathPosition_ = bossSpawnPosition_;
	bossActor_ = nullptr;
	clearItem_.reset();
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossSpawnConditionMet_ = false;
	bossDefeated_ = false;
	bossDeathPositionCaptured_ = false;
	clearItemSpawned_ = false;
	clearItemCollected_ = false;
	isGameClear_ = false;
	cameraShakeTimer_ = cameraShakeDuration_ = cameraShakeAmplitude_ = cameraShakeFrequency_ = cameraShakeSeed_ = 0.0f;
	lastPresentedPhaseRevision_ = 0;
	bossIntroController_.ClearRuntimeBossPositionOverride();
	bossIntroController_.Initialize(bossSpawnPosition_);
	if (mineArenaEnabled_) bossIntroController_.SetRuntimeBossPositionOverride(mineArenaCenter_);
}

void BossBattleController::Finalize(const Dependencies& deps)
{
	DestroyBossActor(deps);
	if (clearItem_ && deps.collisionManager) deps.collisionManager->RemoveCollider(clearItem_.get());
	clearItem_.reset();
	FinalizeMineArena(deps);
	bossIntroController_.Finalize();
}

void BossBattleController::UpdateSpawnProgress(const Dependencies& deps)
{
	if (!deps.crystalManager) return;
	EnsureMineArenaInitialized(deps);
	const bool devicesCompleted = deps.crystalManager->AreAllCrystalsDestroyed();
	if (mineArenaEnabled_)
	{
		minePassageUnlocked_ = devicesCompleted;
		bossSpawnConditionMet_ = devicesCompleted && mineArenaEntered_ && !bossSpawned_;
	}
	else
	{
		bossSpawnConditionMet_ = devicesCompleted && !bossSpawned_;
	}
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
	if (bossIntroController_.ConsumeDebugClearBossParentRequest() && bossActor_)
	{
		bossActor_->ClearRootParentKeepingWorldPosition();
		bossActor_->ForceSyncWorldTransform();
	}
	if (bossIntroController_.ConsumeDebugForceBossToAppearRequest() && bossActor_)
	{
		bossActor_->ClearRootParentKeepingWorldPosition();
		bossActor_->SetPosition(bossIntroController_.GetBossAppearPosition());
		bossActor_->SetYaw(kPi);
		bossActor_->ForceSyncWorldTransform();
	}
	if (bossIntroController_.ConsumeDebugUseGameplayViewProjectionRequest())
	{
		IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (player && player->GetCamera())
		{
			K4E::CameraManager::GetInstance()->SetMainCamera(player->GetCamera());
			player->GetCamera()->Update();
		}
		if (bossActor_) bossActor_->ForceSyncWorldTransform();
	}

	if (!bossIntroController_.IsRunning()) return;
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (camera) K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	bossIntroController_.Update(deltaTime, bossActor_, camera);

	if (bossIntroController_.ConsumeBossSpawnRequest())
	{
		bossSpawnPosition_ = bossIntroController_.GetBossAppearPosition();
		SpawnBossActor(deps, false);
	}
	if (bossIntroController_.ConsumeBossColliderEnableRequest())
	{
		RegisterBossCollider(deps);
		if (player) AlignPlayerViewToBossAfterIntro(*player);
		if (deps.hudManager)
		{
			deps.hudManager->NotifyBossIntroCompleted(bossActor_ ? bossActor_->GetPosition() : bossSpawnPosition_);
			if (stage1BeginnerBalanceEnabled_) deps.hudManager->NotifyStage1BossAppeared();
		}
		StartCameraShake(0.75f, 0.42f, 24.0f);
	}
}

void BossBattleController::UpdateRuntime(const Dependencies& deps, float deltaTime)
{
	EnsureMineArenaInitialized(deps);
	UpdateMineArena(deps, deltaTime);
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	if (bossActor_)
	{
		if (deps.characters) bossActor_->SetTargetActor(deps.characters->GetPlayer());
		if (!bossDeathPositionCaptured_ && bossActor_->IsDead())
		{
			bossDeathPosition_ = bossActor_->GetDeathWorldPosition();
			bossDeathPositionCaptured_ = true; // 部位演出やPhysicsより前にActorが固定した撃破地点を報酬位置の正本にする。
		}
		HandleBossPhasePresentation(deps);
	}
	UpdateCameraShake(deltaTime, player);
	UpdateBossClearProgress(deps, deltaTime);
	if (deps.crystalManager && deps.characters) deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
}

void BossBattleController::UpdatePausedWorld(const Dependencies& deps, float deltaTime)
{
	if (deps.crystalManager && deps.characters) deps.crystalManager->UpdatePresentationOnly(*deps.characters, deltaTime);
	EnsureMineArenaInitialized(deps);
	UpdateMineArena(deps, deltaTime);
	UpdateIntro(deps, deltaTime);
	if (deps.stage) deps.stage->Update();
	UpdateBossClearProgress(deps, 0.0f);
	if (deps.crystalManager && deps.characters) deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
}

void BossBattleController::UpdateHud(const Dependencies& deps, float deltaTime)
{
	(void)deltaTime;
	if (bossActor_) bossActor_->SetHealthHudVisible(IsBossBattleActive());
	if (deps.hudManager) deps.hudManager->SetBossHP(GetBossHP(), GetBossMaxHP(), false); // HP描画はBossActorのGauge Componentへ一本化する。
}

void BossBattleController::UpdateBossGuideHud(IPlayerRuntime& player, HUDManager& hudManager) const
{
	K4E::Camera* camera = player.GetCamera();
	const K4E::Vector3 position = bossActor_ ? bossActor_->GetPosition() : bossSpawnPosition_;
	hudManager.SetBossGuide(player.GetWorldPosition(), position, camera ? camera->GetForward() : K4E::Vector3{ 0.0f, 0.0f, 1.0f }, IsBossBattleActive());
}

void BossBattleController::DrawBoss()
{
	DrawMineArena();
	// 通常描画はCharacterWorldのActorWorld PassがBossActorを他Characterと一緒に描く。
}

void BossBattleController::DrawClearItem(){ if (clearItem_) clearItem_->Draw(); }

void BossBattleController::DrawBossIntro3D()
{
	DrawMineArena();
	if (bossActor_)
	{
		bossActor_->ForceSyncWorldTransform();
		bossActor_->Draw();
	}
}

void BossBattleController::DrawShadow()
{
	DrawMineArenaShadow();
	// 通常ShadowはCharacterWorldのActorWorld Passへ統一する。
}

void BossBattleController::DrawBossIntroShadow()
{
	DrawMineArenaShadow();
	if (bossActor_) bossActor_->DrawShadow();
}

void BossBattleController::DrawImGui(const Dependencies& deps, bool introPresentation)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("ボス状態");
	ImGui::Text("ActorWorld Boss: %s / Collider: %s", bossSpawned_ ? "spawned" : "none", bossColliderRegistered_ ? "enabled" : "disabled");
	ImGui::Text("Intro: %s / Presentation camera: %s", bossIntroController_.IsRunning() ? "active" : "inactive", introPresentation ? "yes" : "no");
	if (mineArenaEnabled_)
	{
		ImGui::Text("Mine passage: %s %.2f / Arena entered: %s / Gate %.2f",
			minePassageUnlocked_ ? "unlocked" : "locked",
			minePassageDoorOpenAmount_,
			mineArenaEntered_ ? "yes" : "no",
			mineArenaGateOpenAmount_);
		ImGui::Text("Mine arena blocks: %zu / Lights: %zu", mineArenaBlocks_.size(), mineCorridorLightIndices_.size() + mineArenaLightIndices_.size());
	}
	bossIntroController_.SetDebugSnapshot(bossActor_, deps.characters && deps.characters->GetPlayerRuntime() ? deps.characters->GetPlayerRuntime()->GetCamera() : nullptr);
	bossIntroController_.DrawImGui();
	if (bossActor_)
	{
		ImGui::Text("HP: %.1f / %.1f / Phase: %d", bossActor_->GetHP(), bossActor_->GetMaxHP(), bossActor_->GetCurrentPhase());
		ImGui::Text("Battle: %s / Dead: %s / Death presentation: %s", bossActor_->IsBattleEnabled() ? "on" : "off", bossActor_->IsDead() ? "yes" : "no", bossActor_->IsDeathPresentationComplete() ? "complete" : "running");
	}
	ImGui::Text("Death position: %.2f %.2f %.2f / Clear item: %s", bossDeathPosition_.x, bossDeathPosition_.y, bossDeathPosition_.z, clearItemSpawned_ ? "spawned" : "none");
#else
	(void)deps; (void)introPresentation;
#endif
}

void BossBattleController::ResetIntroForDebug(const Dependencies& deps)
{
	DestroyBossActor(deps);
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossDefeated_ = false;
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	bossSpawnConditionMet_ = false;
	lastPresentedPhaseRevision_ = 0;
	bossIntroController_.Reset();
}

bool BossBattleController::IsBossBattleActive() const
{
	return bossActor_ && bossColliderRegistered_ && bossActor_->IsAlive() && !bossIntroController_.IsGameplayPaused();
}

float BossBattleController::GetBossHP() const { return bossActor_ ? bossActor_->GetHP() : 0.0f; }
float BossBattleController::GetBossMaxHP() const { return bossActor_ ? bossActor_->GetMaxHP() : 0.0f; }

void BossBattleController::SpawnBossActor(const Dependencies& deps, bool enableBattleImmediately)
{
	if (bossSpawned_ || !deps.characters) return;
	RegisterApplicationActorTypes();
	K4E::ActorWorld& world = deps.characters->GetActorWorld();
	K4E::BossActor* actor = nullptr;
	if (K4E::Actor* prefabActor = world.SpawnActorFromJson(kBossPrefabPath))
	{
		actor = dynamic_cast<K4E::BossActor*>(prefabActor);
		if (!actor) world.DestroyActor(prefabActor);
	}
	if (!actor) actor = &world.SpawnActor<K4E::BossActor>();
	else actor->Initialize();

	bossActor_ = actor;
	bossActor_->SetName("GameplayBossActor");
	bossActor_->SetLayer("Boss");
	bossActor_->AddTag("Boss");
	bossActor_->AddTag("GameplayBoss");
	bossActor_->SetTargetActor(deps.characters->GetPlayer());
	if (stage1BeginnerBalanceEnabled_)
	{
		if (auto* health = bossActor_->GetHealthComponent()) health->ResetHealth(kBeginnerBossMaxHp);
	}
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	bossActor_->SetPosition(enableBattleImmediately ? bossSpawnPosition_ : bossIntroController_.GetBossStartPosition());
	bossActor_->SetYaw(kPi);
	bossActor_->SetBattleEnabled(enableBattleImmediately);
	bossActor_->SetHealthHudVisible(enableBattleImmediately);
	bossActor_->ForceSyncWorldTransform();
	bossSpawned_ = true;
	lastPresentedPhaseRevision_ = bossActor_->GetPhaseRevision();
	if (enableBattleImmediately) RegisterBossCollider(deps);
}

void BossBattleController::RegisterBossCollider(const Dependencies& deps)
{
	if (!bossActor_ || bossColliderRegistered_) return;
	bossActor_->ClearRootParentKeepingWorldPosition();
	bossActor_->SetPosition(bossIntroController_.GetBossAppearPosition());
	bossActor_->SetYaw(kPi);
	bossActor_->SetBattleEnabled(true);
	bossActor_->SetHealthHudVisible(true);
	bossActor_->ForceSyncWorldTransform();
	if (deps.collisionManager && bossActor_->GetCollisionPrimitive()) deps.collisionManager->AddCollider(bossActor_->GetCollisionPrimitive());
	bossColliderRegistered_ = true;
	K4E::Log("[BossActor] Legacy query collider registered.\n");
}

void BossBattleController::DestroyBossActor(const Dependencies& deps)
{
	if (!bossActor_) return;
	if (deps.collisionManager && bossColliderRegistered_ && bossActor_->GetCollisionPrimitive()) deps.collisionManager->RemoveCollider(bossActor_->GetCollisionPrimitive());
	bossActor_->SetBattleEnabled(false);
	bossActor_->SetHealthHudVisible(false);
	bossActor_->SetActive(false);
	if (deps.characters) deps.characters->GetActorWorld().DestroyActor(bossActor_);
	bossActor_ = nullptr;
	bossColliderRegistered_ = false;
}

void BossBattleController::AlignPlayerViewToBossAfterIntro(IPlayerRuntime& player) const
{
	K4E::Camera* camera = player.GetCamera();
	if (!camera) return;
	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTarget(camera->GetTranslate(), bossIntroController_.GetBossLookTarget(), pitch, yaw)) player.SetViewLookAngles(pitch, yaw);
	K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	camera->Update();
}

void BossBattleController::UpdateBossClearProgress(const Dependencies& deps, float deltaTime)
{
	if (bossActor_ && bossActor_->IsDead() && !bossDefeated_)
	{
		bossDefeated_ = true;
		if (!bossDeathPositionCaptured_)
		{
			bossDeathPosition_ = bossActor_->GetDeathWorldPosition();
			bossDeathPositionCaptured_ = true;
		}
		if (deps.setBossDefeated) deps.setBossDefeated(false);
	}
	if (bossDefeated_ && bossActor_ && bossActor_->IsDeathPresentationComplete() && !clearItemSpawned_ && bossDeathPositionCaptured_) SpawnClearItem(deps, bossDeathPosition_);
	if (clearItem_ && !clearItemCollected_)
	{
		clearItem_->Update(deltaTime);
		IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (player && clearItem_->CheckPickup(*player)) CollectClearItem(deps);
	}
}

void BossBattleController::SpawnClearItem(const Dependencies& deps, const K4E::Vector3& deathPosition)
{
	if (clearItemSpawned_) return;
	K4E::Vector3 position = deathPosition;
	if (deps.stage)
	{
		float floorY = -std::numeric_limits<float>::infinity();
		float nearest = std::numeric_limits<float>::infinity();
		for (const K4E::AABB& floor : deps.stage->GetFloorAABBs())
		{
			if (position.x < floor.min.x || position.x > floor.max.x || position.z < floor.min.z || position.z > floor.max.z) continue;
			const float distance = std::abs(floor.max.y - position.y);
			if (distance < nearest){ nearest = distance; floorY = floor.max.y; }
		}
		if (std::isfinite(floorY)) position.y = floorY;
	}
	position.y = std::max(position.y, 0.0f);
	clearItem_ = std::make_unique<BossClearItem>();
	clearItem_->Initialize(position);
	if (deps.collisionManager) deps.collisionManager->AddCollider(clearItem_.get());
	clearItemSpawned_ = true;
	K4E::Log("[GameClear] BossClearItem spawned at BossActor death position.\n");
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
}

void BossBattleController::HandleBossPhasePresentation(const Dependencies& deps)
{
	(void)deps;
	if (!bossActor_) return;
	const unsigned int revision = bossActor_->GetPhaseRevision();
	if (revision == lastPresentedPhaseRevision_) return;
	lastPresentedPhaseRevision_ = revision;
	const int phase = bossActor_->GetCurrentPhase();
	if (phase >= 3) StartCameraShake(0.85f, 0.40f, 25.0f);
	else if (phase >= 2) StartCameraShake(0.55f, 0.24f, 20.0f);
}

void BossBattleController::StartCameraShake(float duration, float amplitude, float frequency)
{
	if (duration <= 0.0f || amplitude <= 0.0f) return;
	cameraShakeDuration_ = cameraShakeTimer_ = duration;
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
	const float rate = std::clamp(cameraShakeTimer_ / cameraShakeDuration_, 0.0f, 1.0f);
	const float t = (cameraShakeDuration_ - cameraShakeTimer_) * cameraShakeFrequency_ + cameraShakeSeed_;
	const float amplitude = cameraShakeAmplitude_ * rate * rate;
	return { std::sin(t * 1.51f) * amplitude, std::cos(t * 1.13f) * amplitude * 0.50f, std::sin(t * 0.83f) * amplitude * 0.30f };
}

bool BossBattleController::CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& pitch, float& yaw)
{
	K4E::Vector3 direction = target - from;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f) return false;
	direction = K4E::Vector3::Normalize(direction);
	yaw = std::atan2(-direction.x, direction.z);
	pitch = std::atan2(-direction.y, std::sqrt(direction.x * direction.x + direction.z * direction.z));
	return true;
}

void BossBattleController::EnsureMineArenaInitialized(const Dependencies& deps)
{
	if (!mineArenaEnabled_ || mineArenaInitialized_) return;
	BuildMinePassageAndArena(deps);
	ApplyMineLighting();
	mineArenaInitialized_ = true;
	K4E::Log("[MineArena] Hidden passage and dome arena initialized.\n");
}

void BossBattleController::FinalizeMineArena(const Dependencies& deps)
{
	if (!mineArenaEnabled_) return;
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (!block.collider) continue;
		if (deps.characters) deps.characters->GetPhysicsWorld().UnregisterCollider(block.collider.get());
		if (deps.collisionManager) deps.collisionManager->RemoveCollider(block.collider.get());
	}
	mineArenaBlocks_.clear();
	mineCorridorLightIndices_.clear();
	mineArenaLightIndices_.clear();
	RestoreMineLighting();
	mineArenaInitialized_ = false;
}

void BossBattleController::BuildMinePassageAndArena(const Dependencies& deps)
{
	const K4E::Vector4 rock{ 0.115f, 0.105f, 0.095f, 1.0f };
	const K4E::Vector4 rockLight{ 0.17f, 0.15f, 0.13f, 1.0f };
	const K4E::Vector4 floorColor{ 0.14f, 0.125f, 0.11f, 1.0f };
	const K4E::Vector4 metal{ 0.19f, 0.205f, 0.215f, 1.0f };

	AddMineArenaBlock(deps, { 0.0f, -1.15f, 42.0f }, { 108.0f, 2.0f, 164.0f }, {}, floorColor, true, "MineGapSealFloor");
	AddMineArenaBlock(deps, { -53.0f, 11.0f, 42.0f }, { 2.5f, 25.0f, 164.0f }, {}, rock, true, "MineGapSealWallLeft");
	AddMineArenaBlock(deps, { 53.0f, 11.0f, 42.0f }, { 2.5f, 25.0f, 164.0f }, {}, rock, true, "MineGapSealWallRight");
	AddMineArenaBlock(deps, { 0.0f, 24.0f, 42.0f }, { 108.0f, 2.5f, 164.0f }, {}, rock, true, "MineGapSealCeiling");

	const float corridorCenterZ = (minePassageDoorCenter_.z + mineArenaGateCenter_.z) * 0.5f;
	const float corridorLength = mineArenaGateCenter_.z - minePassageDoorCenter_.z + 4.0f;
	AddMineArenaBlock(deps, { 0.0f, -0.5f, corridorCenterZ }, { 13.0f, 1.0f, corridorLength }, {}, floorColor, true, "MineHiddenPassageFloor");
	AddMineArenaBlock(deps, { -6.4f, 4.2f, corridorCenterZ }, { 1.4f, 9.5f, corridorLength }, {}, rockLight, true, "MineHiddenPassageWallLeft");
	AddMineArenaBlock(deps, { 6.4f, 4.2f, corridorCenterZ }, { 1.4f, 9.5f, corridorLength }, {}, rockLight, true, "MineHiddenPassageWallRight");
	AddMineArenaBlock(deps, { 0.0f, 8.7f, corridorCenterZ }, { 13.0f, 1.2f, corridorLength }, {}, rock, true, "MineHiddenPassageCeiling");

	AddMineArenaBlock(deps, { -9.0f, 4.2f, minePassageDoorCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineSecretDoorFrameLeft");
	AddMineArenaBlock(deps, { 9.0f, 4.2f, minePassageDoorCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineSecretDoorFrameRight");
	AddMineArenaBlock(deps, { 0.0f, 8.3f, minePassageDoorCenter_.z }, { 18.0f, 2.0f, 3.0f }, {}, rock, true, "MineSecretDoorFrameTop");

	minePassageDoorLeftIndex_ = AddMineArenaBlock(
		deps, { -2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineSecretDoorLeft");
	minePassageDoorRightIndex_ = AddMineArenaBlock(
		deps, { 2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineSecretDoorRight");
	SetMineMovableBlock(minePassageDoorLeftIndex_, { -2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { -7.4f, minePassageDoorCenter_.y, minePassageDoorCenter_.z });
	SetMineMovableBlock(minePassageDoorRightIndex_, { 2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { 7.4f, minePassageDoorCenter_.y, minePassageDoorCenter_.z });

	AddMineArenaBlock(deps, { mineArenaCenter_.x, -0.5f, mineArenaCenter_.z }, { 50.0f, 1.0f, 50.0f }, {}, floorColor, true, "MineBossArenaFloor");

	constexpr int lowerSegmentCount = 24;
	const float lowerRadius = 23.5f;
	const float lowerWidth = (kTwoPi * lowerRadius / static_cast<float>(lowerSegmentCount)) * 1.16f;
	for (int i = 0; i < lowerSegmentCount; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(lowerSegmentCount);
		const bool entranceOpening = std::abs(std::cos(angle) + 1.0f) < 0.10f && std::abs(std::sin(angle)) < 0.42f;
		if (entranceOpening) continue;
		const K4E::Vector3 position{
			mineArenaCenter_.x + std::sin(angle) * lowerRadius,
			5.0f,
			mineArenaCenter_.z + std::cos(angle) * lowerRadius
		};
		AddMineArenaBlock(deps, position, { lowerWidth, 11.0f, 3.0f }, { 0.0f, angle, 0.0f }, rockLight, true, "MineBossArenaLowerWall");
	}

	constexpr int upperSegmentCount = 20;
	const float upperRadius = 18.6f;
	const float upperWidth = (kTwoPi * upperRadius / static_cast<float>(upperSegmentCount)) * 1.18f;
	for (int i = 0; i < upperSegmentCount; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(upperSegmentCount);
		const K4E::Vector3 position{
			mineArenaCenter_.x + std::sin(angle) * upperRadius,
			13.0f,
			mineArenaCenter_.z + std::cos(angle) * upperRadius
		};
		AddMineArenaBlock(deps, position, { upperWidth, 6.5f, 3.2f }, { 0.0f, angle, 0.0f }, rock, true, "MineBossArenaUpperDome");
	}
	AddMineArenaBlock(deps, { mineArenaCenter_.x, 16.9f, mineArenaCenter_.z }, { 38.0f, 2.4f, 38.0f }, {}, rock, true, "MineBossArenaDomeCap");

	AddMineArenaBlock(deps, { -9.0f, 4.2f, mineArenaGateCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineArenaGateFrameLeft");
	AddMineArenaBlock(deps, { 9.0f, 4.2f, mineArenaGateCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineArenaGateFrameRight");
	AddMineArenaBlock(deps, { 0.0f, 8.3f, mineArenaGateCenter_.z }, { 18.0f, 2.0f, 3.0f }, {}, rock, true, "MineArenaGateFrameTop");

	mineArenaGateLeftIndex_ = AddMineArenaBlock(
		deps, { -7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineArenaGateLeft");
	mineArenaGateRightIndex_ = AddMineArenaBlock(
		deps, { 7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineArenaGateRight");
	SetMineMovableBlock(mineArenaGateLeftIndex_, { -2.65f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { -7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z });
	SetMineMovableBlock(mineArenaGateRightIndex_, { 2.65f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { 7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z });
	UpdateMineMovableBlock(mineArenaGateLeftIndex_, 1.0f);
	UpdateMineMovableBlock(mineArenaGateRightIndex_, 1.0f);

	for (int i = 0; i < 8; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / 8.0f;
		const K4E::Vector3 position{
			mineArenaCenter_.x + std::sin(angle) * 19.0f,
			2.0f,
			mineArenaCenter_.z + std::cos(angle) * 19.0f
		};
		AddMineArenaBlock(deps, position, { 2.2f, 5.0f, 2.2f }, { 0.0f, angle, 0.0f }, metal, true, "MineBossArenaSupport");
	}
}

void BossBattleController::UpdateMineArena(const Dependencies& deps, float deltaTime)
{
	if (!mineArenaEnabled_ || !mineArenaInitialized_) return;
	const float passageTarget = minePassageUnlocked_ ? 1.0f : 0.0f;
	minePassageDoorOpenAmount_ = Approach(minePassageDoorOpenAmount_, passageTarget, 0.48f, deltaTime);
	UpdateMineMovableBlock(minePassageDoorLeftIndex_, minePassageDoorOpenAmount_);
	UpdateMineMovableBlock(minePassageDoorRightIndex_, minePassageDoorOpenAmount_);

	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	if (!mineArenaEntered_ && minePassageDoorOpenAmount_ >= 0.92f && player && IsPlayerInsideMineArena(*player))
	{
		mineArenaEntered_ = true;
		StartCameraShake(0.55f, 0.24f, 18.0f);
		K4E::Log("[MineArena] Player entered dome; entrance gate closing.\n");
	}
	const float gateTarget = mineArenaEntered_ ? 0.0f : 1.0f;
	mineArenaGateOpenAmount_ = Approach(mineArenaGateOpenAmount_, gateTarget, 0.70f, deltaTime);
	UpdateMineMovableBlock(mineArenaGateLeftIndex_, mineArenaGateOpenAmount_);
	UpdateMineMovableBlock(mineArenaGateRightIndex_, mineArenaGateOpenAmount_);

	UpdateMineLighting();
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (!block.visual) continue;
		if (deps.shadowLightViewProjection) block.visual->UpdateShadowMatrix(*deps.shadowLightViewProjection);
		block.visual->Update();
	}
}

void BossBattleController::DrawMineArena()
{
	if (!mineArenaEnabled_ || !mineArenaInitialized_) return;
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (block.visual) block.visual->Draw();
	}
}

void BossBattleController::DrawMineArenaShadow()
{
	if (!mineArenaEnabled_ || !mineArenaInitialized_) return;
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (block.visual) block.visual->DrawShadow();
	}
}

size_t BossBattleController::AddMineArenaBlock(
	const Dependencies& deps,
	const K4E::Vector3& position,
	const K4E::Vector3& scale,
	const K4E::Vector3& rotation,
	const K4E::Vector4& color,
	bool collisionEnabled,
	const char* debugName)
{
	MineArenaBlock block{};
	block.closedPosition = position;
	block.openPosition = position;
	block.visual = std::make_unique<K4E::Object3D>();
	block.visual->Initialize(kMineBlockModelPath);
	block.visual->SetScale(scale);
	block.visual->SetRotate(rotation);
	block.visual->SetTranslate(position);
	block.visual->SetColor(color);
	block.visual->SetMetallic(0.05f);
	block.visual->SetRoughness(0.88f);
	block.visual->SetFrustumCullingEnabled(false);
	block.visual->SetIgnoreStageChunkCulling(true);
	block.visual->Update();

	if (collisionEnabled)
	{
		block.collider = std::make_unique<K4E::Collider>();
		ApplyCollisionPreset(*block.collider, ECollisionPresetId::WorldStatic);
		block.collider->SetCenterPosition(position);
		block.collider->SetOBBHalfSize(scale * 0.5f);
		block.collider->SetOrientation(rotation);
		block.collider->SetDebugName(debugName ? debugName : "MineArenaBlock");
		if (deps.characters) deps.characters->GetPhysicsWorld().RegisterCollider(block.collider.get());
		if (deps.collisionManager) deps.collisionManager->AddCollider(block.collider.get());
	}
	mineArenaBlocks_.push_back(std::move(block));
	return mineArenaBlocks_.size() - 1;
}

void BossBattleController::SetMineMovableBlock(size_t blockIndex, const K4E::Vector3& closedPosition, const K4E::Vector3& openPosition)
{
	if (blockIndex >= mineArenaBlocks_.size()) return;
	mineArenaBlocks_[blockIndex].closedPosition = closedPosition;
	mineArenaBlocks_[blockIndex].openPosition = openPosition;
}

void BossBattleController::UpdateMineMovableBlock(size_t blockIndex, float openAmount)
{
	if (blockIndex >= mineArenaBlocks_.size()) return;
	MineArenaBlock& block = mineArenaBlocks_[blockIndex];
	const K4E::Vector3 position = LerpVector(block.closedPosition, block.openPosition, std::clamp(openAmount, 0.0f, 1.0f));
	if (block.visual) block.visual->SetTranslate(position);
	if (block.collider) block.collider->SetCenterPosition(position);
}

bool BossBattleController::IsPlayerInsideMineArena(const IPlayerRuntime& player) const
{
	const K4E::Vector3 position = player.GetWorldPosition();
	const K4E::Vector3 delta = position - mineArenaCenter_;
	return std::abs(delta.x) <= 17.5f && delta.z >= -18.0f && delta.z <= 19.5f && position.y >= -2.0f && position.y <= 12.0f;
}

void BossBattleController::ApplyMineLighting()
{
	if (!mineArenaEnabled_ || mineLightingSaved_) return;
	K4E::LightManager* lightManager = K4E::LightManager::GetInstance();
	auto& lights = lightManager->GetMutablePunctualLightsForEditor();
	auto& settings = lightManager->GetMutableLightingSettingsForEditor();
	savedMineLights_ = lights;
	savedMineLightingSettings_ = settings;
	mineLightingSaved_ = true;

	for (auto& light : lights)
	{
		if (light.lightType == 1) light.intensity *= 0.16f;
	}
	settings.ambientColor = { 0.025f, 0.028f, 0.032f, 0.10f };
	settings.fogColor = { 0.045f, 0.052f, 0.058f, 1.0f };
	settings.exposure = 0.72f;
	settings.contrast = 1.18f;
	settings.fogStart = 16.0f;
	settings.fogEnd = 105.0f;
	settings.enableFog = 1;
	settings.specularStrength = 0.05f;
	settings.diffuseStrength = 0.82f;

	auto addPoint = [&lights](const K4E::Vector3& position, const K4E::Vector4& color, float intensity, float radius)
	{
		K4E::LightManager::PunctualLightGPU light{};
		light.lightType = 2;
		light.color = color;
		light.intensity = intensity;
		light.position = position;
		light.radius = radius;
		light.decay = 2.0f;
		light.direction = { 0.0f, -1.0f, 0.0f };
		light.distance = radius;
		light.cosFalloffStart = 0.86f;
		light.cosAngle = 0.72f;
		light.areaSize = { 0.4f, 0.4f, 0.0f };
		light.enabled = 1;
		lights.push_back(light);
		return lights.size() - 1;
	};
	auto addSpot = [&lights](const K4E::Vector3& position, const K4E::Vector3& direction, const K4E::Vector4& color, float intensity, float distance)
	{
		K4E::LightManager::PunctualLightGPU light{};
		light.lightType = 3;
		light.color = color;
		light.intensity = intensity;
		light.position = position;
		light.radius = distance;
		light.decay = 1.5f;
		light.direction = direction;
		light.distance = distance;
		light.cosFalloffStart = 0.82f;
		light.cosAngle = 0.52f;
		light.areaSize = { 1.0f, 1.0f, 0.0f };
		light.enabled = 1;
		lights.push_back(light);
		return lights.size() - 1;
	};

	mineCorridorLightIndices_.push_back(addPoint({ 0.0f, 6.4f, 52.0f }, { 1.0f, 0.58f, 0.25f, 1.0f }, 0.7f, 13.0f));
	mineCorridorLightIndices_.push_back(addPoint({ 0.0f, 6.4f, 64.0f }, { 1.0f, 0.52f, 0.20f, 1.0f }, 0.7f, 13.0f));
	mineCorridorLightIndices_.push_back(addPoint({ 0.0f, 6.4f, 76.0f }, { 0.72f, 0.82f, 1.0f, 1.0f }, 0.7f, 14.0f));

	mineArenaLightIndices_.push_back(addSpot(
		{ mineArenaCenter_.x, 16.5f, mineArenaCenter_.z - 2.0f },
		{ 0.0f, -1.0f, 0.08f },
		{ 0.58f, 0.72f, 1.0f, 1.0f },
		1.4f,
		32.0f));
	for (int i = 0; i < 4; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / 4.0f + kPi * 0.25f;
		mineArenaLightIndices_.push_back(addPoint(
			{ mineArenaCenter_.x + std::sin(angle) * 16.5f, 5.0f, mineArenaCenter_.z + std::cos(angle) * 16.5f },
			{ 1.0f, 0.36f, 0.16f, 1.0f },
			0.8f,
			18.0f));
	}
}

void BossBattleController::UpdateMineLighting()
{
	if (!mineArenaEnabled_ || !mineLightingSaved_) return;
	K4E::LightManager* lightManager = K4E::LightManager::GetInstance();
	auto& lights = lightManager->GetMutablePunctualLightsForEditor();
	auto& settings = lightManager->GetMutableLightingSettingsForEditor();

	settings.ambientColor = mineArenaEntered_
		? K4E::Vector4{ 0.035f, 0.038f, 0.045f, 0.12f }
		: K4E::Vector4{ 0.022f, 0.024f, 0.028f, 0.09f };
	settings.fogColor = { 0.045f, 0.052f, 0.058f, 1.0f };
	settings.exposure = mineArenaEntered_ ? 0.78f : 0.70f;
	settings.contrast = 1.18f;
	settings.fogStart = 16.0f;
	settings.fogEnd = mineArenaEntered_ ? 125.0f : 100.0f;
	settings.enableFog = 1;

	const float corridorIntensity = 0.55f + minePassageDoorOpenAmount_ * 5.8f;
	for (size_t index : mineCorridorLightIndices_)
	{
		if (index < lights.size()) lights[index].intensity = corridorIntensity;
	}
	const float arenaFactor = mineArenaEntered_ ? 1.0f : (0.12f + minePassageDoorOpenAmount_ * 0.18f);
	for (size_t i = 0; i < mineArenaLightIndices_.size(); ++i)
	{
		const size_t index = mineArenaLightIndices_[i];
		if (index >= lights.size()) continue;
		lights[index].intensity = (i == 0 ? 11.0f : 6.8f) * arenaFactor;
	}
}

void BossBattleController::RestoreMineLighting()
{
	if (!mineLightingSaved_) return;
	K4E::LightManager* lightManager = K4E::LightManager::GetInstance();
	lightManager->GetMutablePunctualLightsForEditor() = savedMineLights_;
	lightManager->GetMutableLightingSettingsForEditor() = savedMineLightingSettings_;
	savedMineLights_.clear();
	mineLightingSaved_ = false;
}

K4E::Vector3 BossBattleController::LerpVector(const K4E::Vector3& a, const K4E::Vector3& b, float t)
{
	return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}
