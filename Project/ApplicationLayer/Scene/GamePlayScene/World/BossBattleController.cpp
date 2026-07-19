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
#include "Input.h"
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
	mineDevices_.clear();
	mineCorridorLightIndices_.clear();
	mineArenaLightIndices_.clear();
	minePassageDoorLeftIndex_ = minePassageDoorRightIndex_ = kInvalidMineBlockIndex;
	mineArenaGateLeftIndex_ = mineArenaGateRightIndex_ = kInvalidMineBlockIndex;
	mineLightingSaved_ = false;
	mineActivatedDeviceCount_ = 0;
	mineFocusedDeviceIndex_ = -1;
	mineRequiredDeviceCount_ = std::max(1, stageContext.GetCurrentStageRule().requiredDeviceCount);

	bossSpawnPosition_ = stageContext.HasBossSpawnPoint()
		? stageContext.GetBossSpawnPoint()
		: (mineArenaEnabled_ ? mineArenaCenter_ : K4E::Vector3{ 0.0f, 2.25f, 30.0f });
	if (mineArenaEnabled_)
	{
		mineArenaCenter_ = bossSpawnPosition_;
		mineArenaCenter_.y = std::max(2.25f, mineArenaCenter_.y);
		minePassageDoorCenter_ = { mineArenaCenter_.x, 3.5f, mineArenaCenter_.z - 60.0f };
		mineArenaGateCenter_ = { mineArenaCenter_.x, 3.5f, mineArenaCenter_.z - 23.0f };
		bossSpawnPosition_ = mineArenaCenter_;
	}
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
	EnsureMineArenaInitialized(deps);
	if (mineArenaEnabled_)
	{
		minePassageUnlocked_ = mineActivatedDeviceCount_ >= mineRequiredDeviceCount_;
		bossSpawnConditionMet_ = minePassageUnlocked_ && mineArenaEntered_ && !bossSpawned_;
	}
	else
	{
		if (!deps.crystalManager) return;
		bossSpawnConditionMet_ = deps.crystalManager->AreAllCrystalsDestroyed() && !bossSpawned_;
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
		ImGui::Text("Devices: %d / %d / Focus: %d", mineActivatedDeviceCount_, mineRequiredDeviceCount_, mineFocusedDeviceIndex_);
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

// 大きくなったBoss/Mine実装は責務ごとの内部includeへ分割し、1つのTranslation Unitとしてビルドする。
#include "BossBattleControllerBossRuntime.inl"
#include "BossBattleControllerMineArena.inl"
#include "BossBattleControllerMineDevices.inl"
#include "BossBattleControllerMineRendering.inl"
