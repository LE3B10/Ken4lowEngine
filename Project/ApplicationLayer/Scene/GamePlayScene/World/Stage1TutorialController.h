#pragma once

#include "CharacterWorld.h"
#include "ItemType.h"
#include <Object3D.h>

#include <functional>

namespace K4E = ::Ken4lowEngine;

class BulletManager;
class CollisionManager;
class CrystalManager;
class HUDManager;
class ItemManager;
class Player;
class EnemyBase;
namespace Ken4lowEngine
{
	class SkyBox;
	class Stage;
}

/// -------------------------------------------------------------
/// ステージ1の初心者向けチュートリアル進行を管理するクラス。
///
/// GamePlayWorldからステージ1専用の説明・操作制限・練習用敵/アイテム生成を切り離し、
/// World本体が通常ゲーム進行に集中できるようにする。
/// -------------------------------------------------------------
class Stage1TutorialController
{
public:
	/// <summary>
	/// チュートリアル更新に必要な外部システム参照をまとめた構造体。
	/// 所有権は持たず、GamePlayWorldが所有している各管理クラスを一時的に参照する。
	/// </summary>
	struct Dependencies
	{
		CharacterWorld* characters = nullptr;
		HUDManager* hudManager = nullptr;
		CrystalManager* crystalManager = nullptr;
		ItemManager* itemManager = nullptr;
		BulletManager* bulletManager = nullptr;
		CollisionManager* collisionManager = nullptr;
		K4E::SkyBox* skyBox = nullptr;
		K4E::Stage* stage = nullptr;
		K4E::Matrix4x4* shadowLightViewProjection = nullptr;
		std::function<void()> collisionUpdate;
		std::function<void()> updateShadowLightViewProjection;
	};

	void Start(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossDefeated, bool skipTutorial = false);
	void Update(const Dependencies& deps, float deltaTime);
	void Finish(const Dependencies& deps);
	void UpdateObjectiveGuideHud(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossBattleActive, bool bossSpawned, bool bossIntroPlayed, bool bossDefeated);

	bool IsActive() const { return objectiveIntroActive_; }
	bool IsTutorialPlaying() const;
	bool IsGameplayBlocked() const;
	bool HasCompletedTutorial() const { return tutorialSeen_; }

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

	void AdvanceStep(const Dependencies& deps);
	void RequestAdvanceStep(float delay);
	void UpdatePendingStepAdvance(const Dependencies& deps, float deltaTime);
	bool AllowsPlayerMove() const;
	bool AllowsPlayerShoot() const;
	bool AllowsReload() const;
	void PrepareReloadPractice(Player& player);
	void ApplyPlayerRestrictions(const Dependencies& deps);
	void SpawnTutorialEnemy(const Dependencies& deps);
	void ClearTutorialEnemy(const Dependencies& deps);
	void SpawnTutorialItems(const Dependencies& deps);
	void UpdateTutorialHud(const Dependencies& deps);
	void AlignPlayerViewToFirstCrystal(const Dependencies& deps, Player& player);
	void UpdatePresentation(const Dependencies& deps, float deltaTime, float tutorialAlpha);

	bool beginnerBalanceEnabled_ = false;
	bool latestBossDefeated_ = false;
	bool objectiveIntroActive_ = false;
	float objectiveIntroTimer_ = 0.0f;
	TutorialStep tutorialStep_ = TutorialStep::None;
	float moveProgress_ = 0.0f;
	float movePracticeTimer_ = 0.0f;
	float movePracticeDistance_ = 0.0f;
	float mouseLookProgress_ = 0.0f;
	float mouseLookPracticeTimer_ = 0.0f;
	float mouseLookAmount_ = 0.0f;
	float shootProgress_ = 0.0f;
	int shootCount_ = 0;
	bool pendingStepAdvance_ = false;
	float stepAdvanceDelayTimer_ = 0.0f;
	float stepAdvanceDelay_ = 0.0f;
	K4E::Vector3 movePreviousPlayerPosition_{};
	K4E::Vector3 savedCameraRotation_{};
	EnemyBase* tutorialEnemy_ = nullptr;
	bool tutorialEnemySpawned_ = false;
	bool tutorialItemSpawned_ = false;
	int tutorialItemsCollected_ = 0;
	bool savedEnemyDeathDropEnabled_ = true;
	bool reloadStarted_ = false;
	bool reloadWasReloading_ = false;
	bool tutorialCompletionNotified_ = false;
	bool tutorialSeen_ = false; // 同じ起動中にチュートリアル完了済みかを保持し、次回スキップ解放に使う。
	float tutorialCompleteTimer_ = 0.0f;
	float tutorialCompleteHoldTime_ = 1.4f;
};
