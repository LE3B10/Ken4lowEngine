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

#include <memory>

namespace K4E = ::Ken4lowEngine;

class GamePlayWorld
{
public: /// ---------- メンバ関数 ---------- ///

	void Initialize(GamePlayStageContext& stageContext);
	void Finalize();

	void Update(float deltaTime);

	void UpdateIntroVisuals();
	void UpdateEquipIntro(float deltaTime);
	void WarmupStartGameplayForIntro();
	void SetStartGameplayVisualsVisible(bool visible);

	void Draw3D(bool hideCharactersDuringIntro);
	void DrawShadow(bool hideCharactersDuringIntro);
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

	void SetDebugCameraEnabled(bool enabled);

	// TODO: 実オブジェクト破壊判定が入るまでの仮API。
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

	bool CheckCrosshairTargetingEnemy() const;

	// TODO: 実オブジェクト接触判定が入るまでの仮API。
	void AddActivatedDeviceCount(int amount = 1);
	// TODO: 実オブジェクト接触判定が入るまでの仮API。
	void SetReachedGoal(bool reached);
	// TODO: 実オブジェクト接触判定が入るまでの仮API。
	void SetBossDefeated(bool defeated);

private: /// ---------- メンバ関数 ---------- ///

	void CollisionUpdate();
	void UpdateShadowLightViewProjection();
	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const;
	bool IsSightBlocked(const K4E::Segment& seg) const;


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
};
