#pragma once

#include "BossIntroController.h"
#include "Collider.h"
#include "LightManager.h"
#include "Object3D.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	class BossActor;
	class Matrix4x4;
	class Stage;
}

namespace K4E = ::Ken4lowEngine;

class IPlayerRuntime;
class CharacterWorld;
class CollisionManager;
class CrystalManager;
class GamePlayStageContext;
class HUDManager;

class BossClearItem : public K4E::Collider
{
public:
	void Initialize(const K4E::Vector3& position);
	void Update(float deltaTime);
	void Draw();
	bool CheckPickup(const IPlayerRuntime& player) const;
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

/// ボス出現、登場演出、ActorWorld所有BossActor、撃破後クリアItemを管理する。
class BossBattleController
{
public:
	struct Dependencies
	{
		CharacterWorld* characters = nullptr;
		HUDManager* hudManager = nullptr;
		CrystalManager* crystalManager = nullptr;
		CollisionManager* collisionManager = nullptr;
		K4E::Stage* stage = nullptr;
		K4E::Matrix4x4* shadowLightViewProjection = nullptr;
		std::function<void(bool)> setBossDefeated;
		std::function<void()> updateShadowLightViewProjection;
	};

	void Initialize(GamePlayStageContext& stageContext, bool stage1BeginnerBalanceEnabled);
	void Finalize(const Dependencies& deps);
	void UpdateSpawnProgress(const Dependencies& deps);
	void UpdateIntro(const Dependencies& deps, float deltaTime);
	void UpdateRuntime(const Dependencies& deps, float deltaTime);
	void UpdatePausedWorld(const Dependencies& deps, float deltaTime);
	void UpdateHud(const Dependencies& deps, float deltaTime);
	void UpdateBossGuideHud(IPlayerRuntime& player, HUDManager& hudManager) const;

	void DrawBoss();
	void DrawClearItem();
	void DrawBossIntro3D();
	void DrawShadow();
	void DrawBossIntroShadow();
	void DrawImGui(const Dependencies& deps, bool bossIntroPresentationActive);
	void ResetIntroForDebug(const Dependencies& deps);
	void SetBossDefeated(bool defeated) { bossDefeated_ = defeated; }

	bool IsGameClearRequested() const { return isGameClear_; }
	bool IsIntroActive() const { return bossIntroController_.IsRunning(); }
	bool IsIntroGameplayPaused() const { return bossIntroController_.IsGameplayPaused(); }
	bool IsIntroPresentationActive() const { return bossIntroController_.IsGameplayPaused(); }
	bool IsSpawned() const { return bossSpawned_; }
	bool IsColliderRegistered() const { return bossColliderRegistered_; }
	bool IsSpawnConditionMet() const { return bossSpawnConditionMet_; }
	bool IsDefeated() const { return bossDefeated_; }
	bool HasIntroPlayed() const { return bossIntroController_.HasPlayed(); }
	bool IsBossBattleActive() const;
	float GetBossHP() const;
	float GetBossMaxHP() const;
	const K4E::Vector3& GetBossSpawnPosition() const { return bossSpawnPosition_; }
	K4E::BossActor* GetBoss() const { return bossActor_; }

private:
	struct MineArenaBlock
	{
		std::unique_ptr<K4E::Object3D> visual;
		std::unique_ptr<K4E::Collider> collider;
		K4E::Vector3 closedPosition{};
		K4E::Vector3 openPosition{};
	};

	void SpawnBossActor(const Dependencies& deps, bool enableBattleImmediately);
	void RegisterBossCollider(const Dependencies& deps);
	void DestroyBossActor(const Dependencies& deps);
	void AlignPlayerViewToBossAfterIntro(IPlayerRuntime& player) const;
	void UpdateBossClearProgress(const Dependencies& deps, float deltaTime);
	void SpawnClearItem(const Dependencies& deps, const K4E::Vector3& bossPosition);
	void CollectClearItem(const Dependencies& deps);
	void HandleBossPhasePresentation(const Dependencies& deps);
	void StartCameraShake(float duration, float amplitude, float frequency);
	void UpdateCameraShake(float deltaTime, IPlayerRuntime* player);
	K4E::Vector3 BuildCameraShakeOffset() const;
	static bool CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& outPitch, float& outYaw);

	void EnsureMineArenaInitialized(const Dependencies& deps);
	void FinalizeMineArena(const Dependencies& deps);
	void BuildMinePassageAndArena(const Dependencies& deps);
	void UpdateMineArena(const Dependencies& deps, float deltaTime);
	void DrawMineArena();
	void DrawMineArenaShadow();
	size_t AddMineArenaBlock(
		const Dependencies& deps,
		const K4E::Vector3& position,
		const K4E::Vector3& scale,
		const K4E::Vector3& rotation,
		const K4E::Vector4& color,
		bool collisionEnabled,
		const char* debugName);
	void SetMineMovableBlock(size_t blockIndex, const K4E::Vector3& closedPosition, const K4E::Vector3& openPosition);
	void UpdateMineMovableBlock(size_t blockIndex, float openAmount);
	bool IsPlayerInsideMineArena(const IPlayerRuntime& player) const;
	void ApplyMineLighting();
	void UpdateMineLighting();
	void RestoreMineLighting();
	static K4E::Vector3 LerpVector(const K4E::Vector3& a, const K4E::Vector3& b, float t);

private:
	static constexpr size_t kInvalidMineBlockIndex = static_cast<size_t>(-1);
	K4E::BossActor* bossActor_ = nullptr; // 実体はCharacterWorldのActorWorldが所有する。
	std::unique_ptr<BossClearItem> clearItem_;
	K4E::Vector3 bossSpawnPosition_{ 0.0f, 2.25f, 30.0f };
	K4E::Vector3 bossDeathPosition_{};
	BossIntroController bossIntroController_;
	bool stage1BeginnerBalanceEnabled_ = false;
	bool bossSpawned_ = false;
	bool bossColliderRegistered_ = false;
	bool bossSpawnConditionMet_ = false;
	bool bossDefeated_ = false;
	bool bossDeathPositionCaptured_ = false;
	bool clearItemSpawned_ = false;
	bool clearItemCollected_ = false;
	bool isGameClear_ = false;
	float cameraShakeTimer_ = 0.0f;
	float cameraShakeDuration_ = 0.0f;
	float cameraShakeAmplitude_ = 0.0f;
	float cameraShakeFrequency_ = 0.0f;
	float cameraShakeSeed_ = 0.0f;
	unsigned int lastPresentedPhaseRevision_ = 0;

	bool mineArenaEnabled_ = false;
	bool mineArenaInitialized_ = false;
	bool minePassageUnlocked_ = false;
	bool mineArenaEntered_ = false;
	float minePassageDoorOpenAmount_ = 0.0f;
	float mineArenaGateOpenAmount_ = 1.0f;
	K4E::Vector3 mineArenaCenter_{ 0.0f, 2.25f, 105.0f };
	K4E::Vector3 minePassageDoorCenter_{ 0.0f, 3.5f, 45.0f };
	K4E::Vector3 mineArenaGateCenter_{ 0.0f, 3.5f, 82.0f };
	std::vector<MineArenaBlock> mineArenaBlocks_;
	size_t minePassageDoorLeftIndex_ = kInvalidMineBlockIndex;
	size_t minePassageDoorRightIndex_ = kInvalidMineBlockIndex;
	size_t mineArenaGateLeftIndex_ = kInvalidMineBlockIndex;
	size_t mineArenaGateRightIndex_ = kInvalidMineBlockIndex;
	std::vector<K4E::LightManager::PunctualLightGPU> savedMineLights_;
	K4E::LightManager::LightingSettingsGPU savedMineLightingSettings_{};
	std::vector<size_t> mineCorridorLightIndices_;
	std::vector<size_t> mineArenaLightIndices_;
	bool mineLightingSaved_ = false;
};
