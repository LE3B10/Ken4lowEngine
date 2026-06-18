#pragma once

#include "BossIntroController.h"
#include "GuardianBoss.h"
#include "Collider.h"
#include "Object3D.h"
#include "Vector3.h"

#include <functional>
#include <memory>

namespace Ken4lowEngine
{
	class Matrix4x4;
	class Stage;
}

namespace K4E = ::Ken4lowEngine;

class CharacterWorld;
class CollisionManager;
class CrystalManager;
class GamePlayStageContext;
class HUDManager;
class Player;

/// -------------------------------------------------------------
/// ボス撃破後に出現するクリア用アイテム。
///
/// BossBattleControllerが生成・所有し、取得後は判定を無効化して
/// ゲームクリア進行へ通知するための小さなCollider。
/// -------------------------------------------------------------
class BossClearItem : public K4E::Collider
{
public:
	// ボス位置を基準に表示モデルとItem判定を生成する。CollisionManager登録は呼び出し側が行う。
	void Initialize(const K4E::Vector3& position);
	// 浮遊・回転演出とCollider中心を同期する。
	void Update(float deltaTime);
	// 未取得かつ生成済みのときだけモデルを描画する。
	void Draw();
	// プレイヤー中心との距離で取得可能か判定する。実取得処理はController側で行う。
	bool CheckPickup(const Player& player) const;
	// 取得済みにして判定を遠方へ逃がす。CollisionManagerからの削除は呼び出し側が行う。
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

/// -------------------------------------------------------------
/// ボス戦の出現、登場演出、撃破後クリアアイテムを管理するクラス。
///
/// GamePlayWorldからボス専用の状態と処理を切り離し、World本体が
/// 通常更新・描画・各Manager連携に集中できるようにする。
/// -------------------------------------------------------------
class BossBattleController
{
public:
	/// GamePlayWorld側が所有しているManager群への一時参照。
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
	void UpdateBossGuideHud(Player& player, HUDManager& hudManager) const;

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
	GuardianBoss* GetBoss() const { return guardianBoss_.get(); }

private:
	void SpawnGuardianBoss(const Dependencies& deps, bool registerCollider);
	void RegisterGuardianBossCollider(const Dependencies& deps);
	void AlignPlayerViewToBossAfterIntro(Player& player) const;
	void UpdateBossClearProgress(const Dependencies& deps, float deltaTime);
	void SpawnClearItem(const Dependencies& deps, const K4E::Vector3& bossPosition);
	void CollectClearItem(const Dependencies& deps);
	static bool CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& outPitch, float& outYaw);

private:
	std::unique_ptr<GuardianBoss> guardianBoss_;
	std::unique_ptr<BossClearItem> clearItem_;
	K4E::Vector3 bossSpawnPosition_{ 0.0f, 2.25f, 30.0f };
	BossIntroController bossIntroController_;
	bool stage1BeginnerBalanceEnabled_ = false;
	bool bossSpawned_ = false;
	bool bossColliderRegistered_ = false;
	bool bossSpawnConditionMet_ = false;
	bool bossDefeated_ = false;
	bool clearItemSpawned_ = false;
	bool clearItemCollected_ = false;
	bool isGameClear_ = false;
};
