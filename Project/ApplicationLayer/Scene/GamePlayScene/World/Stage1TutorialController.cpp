#define NOMINMAX
#include "Stage1TutorialController.h"

#include "BulletManager.h"
#include "CollisionManager.h"
#include "CrystalManager.h"
#include "EnemyBase.h"
#include "EnemyHPBarProjector.h"
#include "GameViewportConstants.h"
#include "HUDManager.h"
#include "Input.h"
#include "ItemManager.h"
#include "Player.h"
#include "Stage.h"
#include <SkyBox.h>

#include <algorithm>
#include <cmath>

namespace
{
	bool CalcLookAnglesToTargetForStage1(const K4E::Vector3& from, const K4E::Vector3& target, float& outPitch, float& outYaw)
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
}

void Stage1TutorialController::Start(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossDefeated)
{
	if (!beginnerBalanceEnabled || !deps.hudManager || !deps.crystalManager || !deps.itemManager)
	{
		return;
	}

	beginnerBalanceEnabled_ = beginnerBalanceEnabled;
	latestBossDefeated_ = bossDefeated;

	// ステージ1は導入ステージなので、開始直後に目的表示と最初のクリスタル方向を案内する。
	objectiveIntroActive_ = true;
	objectiveIntroTimer_ = 0.0f;
	tutorialStep_ = TutorialStep::CrystalExplanation;
	moveProgress_ = 0.0f;
	mouseLookProgress_ = 0.0f;
	shootProgress_ = 0.0f;
	shootCount_ = 0;
	tutorialEnemy_ = nullptr;
	tutorialEnemySpawned_ = false;
	tutorialItemSpawned_ = false;
	tutorialItemsCollected_ = 0;
	savedEnemyDeathDropEnabled_ = deps.itemManager->IsEnemyDeathDropEnabled();
	deps.itemManager->SetEnemyDeathDropEnabled(false);
	reloadStarted_ = false;
	reloadWasReloading_ = false;
	tutorialCompletionNotified_ = false;
	tutorialCompleteTimer_ = 0.0f;
	deps.hudManager->SetStage1ObjectiveGuide(
		true,
		deps.crystalManager->GetDestroyedCrystalCount(),
		deps.crystalManager->GetCrystalCount(),
		false,
		bossDefeated,
		true);
	deps.hudManager->SetStage1ObjectiveTutorialAlpha(1.0f);
	deps.hudManager->SetStage1ObjectiveTutorialPage(0);
	deps.hudManager->SetStage1ObjectiveTutorialProgress(0.0f);
	deps.hudManager->NotifyStage1ObjectiveGuideStarted();
	if (deps.characters)
	{
		if (auto* player = deps.characters->GetPlayer())
		{
			if (auto* camera = player->GetCamera())
			{
				savedCameraRotation_ = camera->GetRotate();
			}
			AlignPlayerViewToFirstCrystal(deps, *player);
			movePreviousPlayerPosition_ = player->GetCenterPosition();
		}
	}
}

void Stage1TutorialController::Update(const Dependencies& deps, float deltaTime)
{
	if (!objectiveIntroActive_ || !deps.characters || !deps.crystalManager)
	{
		return;
	}

	objectiveIntroTimer_ += deltaTime;
	float tutorialAlpha = 1.0f;

	deps.crystalManager->UpdatePresentationOnly(*deps.characters, deltaTime);
	deps.crystalManager->SetFirstAliveCrystalGuideHighlight(
		tutorialStep_ == TutorialStep::CrystalExplanation ? tutorialAlpha : 0.0f);

	auto* input = K4E::Input::GetInstance();
	const bool clickedNext = input && (input->TriggerMouse(0) || input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerButton(K4E::XButtons.A));
	if (tutorialStep_ == TutorialStep::CrystalExplanation && clickedNext)
	{
		// 目的説明中のクリックは射撃ではなく、チュートリアル進行入力として扱う。
		AdvanceStep(deps);
	}

	ApplyPlayerRestrictions(deps);
	if (auto* player = deps.characters->GetPlayer())
	{
		if (tutorialStep_ == TutorialStep::MovePractice)
		{
			const K4E::Vector3 before = player->GetCenterPosition();
			deps.characters->UpdatePlayerOnly(deltaTime);
			const K4E::Vector3 after = player->GetCenterPosition();
			const K4E::Vector3 delta = after - before;
			const float movedDistance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
			// 移動操作を実際に行わせ、初心者がWASD移動を覚えてから次へ進める。
			moveProgress_ = std::clamp(moveProgress_ + movedDistance / 4.0f, 0.0f, 1.0f);
			movePreviousPlayerPosition_ = after;
			if (moveProgress_ >= 1.0f)
			{
				AdvanceStep(deps);
			}
		}
		else if (tutorialStep_ == TutorialStep::MouseLookPractice)
		{
			deps.characters->UpdatePlayerOnly(deltaTime);
			const float mouseLookAmount = input
				? (std::fabs(static_cast<float>(input->GetMouseMoveX())) + std::fabs(static_cast<float>(input->GetMouseMoveY())))
				: 0.0f;
			// FPS操作に必要な視点移動を覚えさせるため、マウス移動量で進捗を進める。
			mouseLookProgress_ = std::clamp(mouseLookProgress_ + mouseLookAmount / 600.0f, 0.0f, 1.0f);
			if (mouseLookProgress_ >= 1.0f)
			{
				AdvanceStep(deps);
			}
		}
		else if (tutorialStep_ == TutorialStep::ItemPickupPractice)
		{
			SpawnTutorialItems(deps);
			deps.characters->UpdatePlayerOnly(deltaTime);
			if (deps.itemManager)
			{
				deps.itemManager->Update(player);
				// アイテムを2つ実際に拾わせ、取得方法と効果を理解させる。
				if (deps.itemManager->ConsumeCollected(ItemType::AmmoSmall))
				{
					++tutorialItemsCollected_;
				}
				if (deps.itemManager->ConsumeCollected(ItemType::HealSmall))
				{
					++tutorialItemsCollected_;
				}
			}
			if (tutorialItemsCollected_ >= 2)
			{
				AdvanceStep(deps);
			}
		}
		else if (tutorialStep_ == TutorialStep::ShootPractice)
		{
			const int magazineBefore = player->GetCurrentWeaponMagazineAmmo();
			deps.characters->UpdatePlayerOnly(deltaTime);
			const int magazineAfter = player->GetCurrentWeaponMagazineAmmo();
			// 説明後の左クリックは射撃入力として扱い、実際に弾が出た回数で練習進捗を進める。
			if (magazineAfter < magazineBefore)
			{
				++shootCount_;
				shootProgress_ = std::clamp(static_cast<float>(shootCount_) / 3.0f, 0.0f, 1.0f);
				player->AddReserveAmmo(3);
			}
			if (deps.bulletManager)
			{
				deps.bulletManager->Update(deltaTime);
			}
			if (shootProgress_ >= 1.0f)
			{
				AdvanceStep(deps);
			}
		}
		else if (tutorialStep_ == TutorialStep::ReloadPractice)
		{
			deps.characters->UpdatePlayerOnly(deltaTime);
			bool isReloading = false;
			float reloadTimer = 0.0f;
			float reloadSec = 0.0f;
			player->GetReloadUI(isReloading, reloadTimer, reloadSec);
			if (isReloading)
			{
				reloadStarted_ = true;
			}
			// リロード操作を確実に覚えさせるため、開始ではなく完了を検知して次へ進める。
			if (reloadStarted_ && reloadWasReloading_ && !isReloading)
			{
				AdvanceStep(deps);
			}
			reloadWasReloading_ = isReloading;
		}
		else if (tutorialStep_ == TutorialStep::EnemyPractice)
		{
			SpawnTutorialEnemy(deps);
			deps.characters->Update(deltaTime);
			if (deps.bulletManager)
			{
				deps.bulletManager->Update(deltaTime);
			}
			if (deps.collisionManager && deps.collisionUpdate)
			{
				deps.collisionUpdate();
			}
			if (tutorialEnemy_ && tutorialEnemy_->IsDead())
			{
				AdvanceStep(deps);
			}
		}
		else if (tutorialStep_ == TutorialStep::Completed)
		{
			tutorialCompleteTimer_ += deltaTime;
			tutorialAlpha = 1.0f - std::clamp(tutorialCompleteTimer_ / std::max(0.01f, tutorialCompleteHoldTime_), 0.0f, 1.0f);
			if (tutorialCompleteTimer_ >= tutorialCompleteHoldTime_)
			{
				Finish(deps);
				return;
			}
		}
	}

	UpdatePresentation(deps, deltaTime, tutorialAlpha);
}

void Stage1TutorialController::Finish(const Dependencies& deps)
{
	objectiveIntroActive_ = false;
	tutorialStep_ = TutorialStep::Completed;
	if (deps.crystalManager)
	{
		deps.crystalManager->SetFirstAliveCrystalGuideHighlight(0.0f);
	}
	if (deps.itemManager)
	{
		deps.itemManager->SetEnemyDeathDropEnabled(savedEnemyDeathDropEnabled_);
		deps.itemManager->SetConsumeItemWhenFull(false);
	}
	if (deps.characters)
	{
		if (auto* player = deps.characters->GetPlayer())
		{
			player->SetTutorialInputRestrictions(false, true, true, true);
		}
	}
	if (deps.hudManager)
	{
		deps.hudManager->SetStage1ObjectiveTutorialAlpha(0.0f);
		deps.hudManager->SetStage1ObjectiveTutorialPage(0);
		deps.hudManager->SetStage1ObjectiveTutorialProgress(0.0f);
		deps.hudManager->SetStage1TutorialItemMarker(0, false, {}, 0);
		deps.hudManager->SetStage1TutorialItemMarker(1, false, {}, 0);
	}
	UpdateObjectiveGuideHud(deps, beginnerBalanceEnabled_, false, false, false, latestBossDefeated_);
}

void Stage1TutorialController::UpdateObjectiveGuideHud(
	const Dependencies& deps,
	bool beginnerBalanceEnabled,
	bool bossBattleActive,
	bool bossSpawned,
	bool bossIntroPlayed,
	bool bossDefeated)
{
	latestBossDefeated_ = bossDefeated;
	if (!deps.hudManager || !deps.crystalManager)
	{
		return;
	}

	deps.hudManager->SetStage1ObjectiveGuide(
		beginnerBalanceEnabled,
		deps.crystalManager->GetDestroyedCrystalCount(),
		deps.crystalManager->GetCrystalCount(),
		bossBattleActive || bossSpawned || bossIntroPlayed,
		bossDefeated,
		objectiveIntroActive_);
}

bool Stage1TutorialController::IsTutorialPlaying() const
{
	return objectiveIntroActive_ && tutorialStep_ != TutorialStep::None && tutorialStep_ != TutorialStep::Completed;
}

bool Stage1TutorialController::IsGameplayBlocked() const
{
	return objectiveIntroActive_ && tutorialStep_ != TutorialStep::Completed;
}

void Stage1TutorialController::AdvanceStep(const Dependencies& deps)
{
	switch (tutorialStep_)
	{
	case TutorialStep::CrystalExplanation:
		tutorialStep_ = TutorialStep::MovePractice;
		if (deps.characters)
		{
			if (auto* player = deps.characters->GetPlayer())
			{
				movePreviousPlayerPosition_ = player->GetCenterPosition();
			}
		}
		break;
	case TutorialStep::MovePractice:
		tutorialStep_ = TutorialStep::MouseLookPractice;
		mouseLookProgress_ = 0.0f;
		break;
	case TutorialStep::MouseLookPractice:
		tutorialStep_ = TutorialStep::ShootPractice;
		break;
	case TutorialStep::ShootPractice:
		tutorialStep_ = TutorialStep::ReloadPractice;
		reloadStarted_ = false;
		reloadWasReloading_ = false;
		break;
	case TutorialStep::ReloadPractice:
		tutorialStep_ = TutorialStep::EnemyPractice;
		break;
	case TutorialStep::EnemyPractice:
		// 敵撃破練習が終わったら、アイテム練習へ入る前にチュートリアル敵を完全に片付ける。
		ClearTutorialEnemy(deps);
		tutorialStep_ = TutorialStep::ItemPickupPractice;
		tutorialItemsCollected_ = 0;
		tutorialItemSpawned_ = false;
		break;
	case TutorialStep::ItemPickupPractice:
		tutorialStep_ = TutorialStep::Completed;
		tutorialCompleteTimer_ = 0.0f;
		tutorialCompletionNotified_ = true;
		break;
	default:
		break;
	}
	objectiveIntroTimer_ = 0.0f;
}

bool Stage1TutorialController::AllowsPlayerMove() const
{
	return tutorialStep_ == TutorialStep::ItemPickupPractice ||
		tutorialStep_ == TutorialStep::MovePractice ||
		tutorialStep_ == TutorialStep::MouseLookPractice ||
		tutorialStep_ == TutorialStep::ShootPractice ||
		tutorialStep_ == TutorialStep::ReloadPractice ||
		tutorialStep_ == TutorialStep::EnemyPractice;
}

bool Stage1TutorialController::AllowsPlayerShoot() const
{
	return tutorialStep_ == TutorialStep::ShootPractice ||
		tutorialStep_ == TutorialStep::EnemyPractice;
}

bool Stage1TutorialController::AllowsReload() const
{
	return tutorialStep_ == TutorialStep::ReloadPractice ||
		tutorialStep_ == TutorialStep::EnemyPractice;
}

void Stage1TutorialController::ApplyPlayerRestrictions(const Dependencies& deps)
{
	if (!deps.characters)
	{
		return;
	}

	if (auto* player = deps.characters->GetPlayer())
	{
		// チュートリアル中は本番のゲーム進行を止め、現在の練習ステップだけを許可する。
		player->SetTutorialInputRestrictions(
			objectiveIntroActive_,
			AllowsPlayerMove(),
			AllowsPlayerShoot(),
			AllowsReload());
	}
}

void Stage1TutorialController::SpawnTutorialEnemy(const Dependencies& deps)
{
	if (tutorialEnemySpawned_ || !deps.characters)
	{
		return;
	}
	auto* player = deps.characters->GetPlayer();
	if (!player)
	{
		return;
	}
	const K4E::Vector3 playerPos = player->GetCenterPosition();
	const K4E::Vector3 spawnPosition{ playerPos.x, playerPos.y, playerPos.z + 8.0f };
	// 本番開始前に弱い敵を1体倒させ、射撃とリロードの流れを確認させる。
	EnemyBase& enemy = deps.characters->SpawnEnemyAt(spawnPosition, EnemyType::Melee);
	enemy.SetMaxHp(60);
	enemy.SetCurrentHp(60);
	tutorialEnemy_ = &enemy;
	tutorialEnemySpawned_ = true;
}

void Stage1TutorialController::ClearTutorialEnemy(const Dependencies& deps)
{
	if (tutorialEnemy_ && deps.characters)
	{
		// アイテム取得練習では敵を残さず、プレイヤーが拾う対象に集中できるようにする。
		deps.characters->RemoveEnemy(tutorialEnemy_);
	}
	tutorialEnemy_ = nullptr;
	tutorialEnemySpawned_ = false;
}

void Stage1TutorialController::SpawnTutorialItems(const Dependencies& deps)
{
	if (tutorialItemSpawned_ || !deps.characters || !deps.itemManager)
	{
		return;
	}
	auto* player = deps.characters->GetPlayer();
	if (!player)
	{
		return;
	}
	const K4E::Vector3 playerPos = player->GetCenterPosition();
	deps.itemManager->SetConsumeItemWhenFull(true);
	// アイテムを2つ実際に拾わせ、取得方法と効果を理解させる。
	deps.itemManager->SpawnAmmoSmall({ playerPos.x + 2.2f, playerPos.y, playerPos.z + 4.0f });
	deps.itemManager->SpawnHealSmall({ playerPos.x - 2.2f, playerPos.y, playerPos.z + 4.0f });
	deps.itemManager->RegisterColliders(deps.collisionManager);
	tutorialItemSpawned_ = true;
}

void Stage1TutorialController::UpdateTutorialHud(const Dependencies& deps)
{
	if (!deps.hudManager || !deps.itemManager || !deps.characters)
	{
		return;
	}

	int page = 0;
	float progress = 0.0f;
	switch (tutorialStep_)
	{
	case TutorialStep::CrystalExplanation: page = 0; break;
	case TutorialStep::MovePractice:
		page = 1;
		progress = moveProgress_;
		break;
	case TutorialStep::MouseLookPractice:
		page = 2;
		progress = mouseLookProgress_;
		break;
	case TutorialStep::ShootPractice:
		page = 3;
		progress = shootProgress_;
		break;
	case TutorialStep::ReloadPractice: page = 4; break;
	case TutorialStep::EnemyPractice: page = 5; break;
	case TutorialStep::ItemPickupPractice:
		page = 6;
		progress = std::clamp(static_cast<float>(tutorialItemsCollected_) / 2.0f, 0.0f, 1.0f);
		break;
	case TutorialStep::Completed: page = 7; break;
	default: break;
	}
	deps.hudManager->SetStage1ObjectiveTutorialPage(page);
	deps.hudManager->SetStage1ObjectiveTutorialProgress(progress);

	deps.hudManager->SetStage1TutorialItemMarker(0, false, {}, 0);
	deps.hudManager->SetStage1TutorialItemMarker(1, false, {}, 0);
	if (tutorialStep_ == TutorialStep::ItemPickupPractice)
	{
		auto projectMarker = [&deps](int markerIndex, ItemType itemType, int markerType)
		{
			K4E::Vector3 itemPosition{};
			if (!deps.itemManager->TryGetFirstActiveItemPosition(itemType, itemPosition))
			{
				return;
			}
			if (auto* player = deps.characters->GetPlayer())
			{
				if (auto* camera = player->GetCamera())
				{
					const float width = static_cast<float>(K4E::GameViewportConstants::Width);
					const float height = static_cast<float>(K4E::GameViewportConstants::Height);
					const K4E::Vector3 markerWorld{ itemPosition.x, itemPosition.y + 0.5f, itemPosition.z };
					const HpBarProjectResult projected = ProjectWorldToScreen(markerWorld, camera->GetViewMatrix(), camera->GetProjectionMatrix(), width, height);
					deps.hudManager->SetStage1TutorialItemMarker(markerIndex, projected.inFront && projected.inScreen, projected.screenPos, markerType);
				}
			}
		};
		projectMarker(0, ItemType::AmmoSmall, 1);
		projectMarker(1, ItemType::HealSmall, 0);
	}
}

void Stage1TutorialController::AlignPlayerViewToFirstCrystal(const Dependencies& deps, Player& player)
{
	auto* camera = player.GetCamera();
	if (!camera || !deps.crystalManager)
	{
		return;
	}

	K4E::Vector3 crystalPosition{};
	if (!deps.crystalManager->TryGetFirstAliveCrystalPosition(crystalPosition))
	{
		return;
	}

	crystalPosition.y += 1.8f;
	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTargetForStage1(camera->GetTranslate(), crystalPosition, pitch, yaw))
	{
		player.SetViewLookAngles(pitch, yaw);
		player.SyncViewToPlayer();
		camera->Update();
	}
}

void Stage1TutorialController::UpdatePresentation(const Dependencies& deps, float deltaTime, float tutorialAlpha)
{
	if (deps.skyBox)
	{
		deps.skyBox->Update();
		deps.skyBox->AdvanceCloudLayer(deltaTime);
	}

	if (deps.updateShadowLightViewProjection)
	{
		deps.updateShadowLightViewProjection();
	}
	if (deps.stage && deps.shadowLightViewProjection)
	{
		deps.stage->UpdateShadowMatrix(*deps.shadowLightViewProjection);
	}
	if (deps.characters && deps.shadowLightViewProjection)
	{
		deps.characters->UpdateShadowMatrix(*deps.shadowLightViewProjection);
	}

	if (deps.hudManager && deps.characters && deps.crystalManager)
	{
		if (auto* player = deps.characters->GetPlayer())
		{
			if (auto* camera = player->GetCamera())
			{
				const float width = static_cast<float>(K4E::GameViewportConstants::Width);
				const float height = static_cast<float>(K4E::GameViewportConstants::Height);
				deps.crystalManager->UpdateHpBars(
					camera->GetViewMatrix(),
					camera->GetProjectionMatrix(),
					width,
					height,
					deltaTime,
					tutorialStep_ == TutorialStep::CrystalExplanation ? deps.crystalManager->GetFirstAliveCrystal() : nullptr,
					true,
					0.3f);
			}
			deps.hudManager->SetHP(player->GetHP(), player->GetMaxHP());
		}
		deps.hudManager->SetBossHP(0.0f, 0.0f, false);
		deps.hudManager->SetStage1ObjectiveTutorialAlpha(tutorialAlpha);
		UpdateTutorialHud(deps);
		deps.hudManager->Update(deltaTime);
	}
}
