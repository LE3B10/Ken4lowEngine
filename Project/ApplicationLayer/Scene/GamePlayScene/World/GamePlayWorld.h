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
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "Object3D.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

class Player;

/// -------------------------------------------------------------
/// ボス撃破後に出現するクリア用アイテム
///
/// GamePlayWorldが生成・所有し、CollisionManagerへ登録する。
/// 取得後は遠方へ移動して当たり判定から外し、World側のクリア判定へ通知する。
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
	// プレイヤー中心との距離で取得可能か判定する。実取得処理はGamePlayWorld側で行う。
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
/// GamePlayScene内のランタイムWorld管理クラス
///
/// ステージ、キャラクター、弾、HUD、Wave、クリスタル、ボス、アイテム、衝突を所有し、
/// GamePlaySceneから1フレームごとに更新・描画される。
/// GamePlaySceneより短い寿命で、リトライ時には丸ごと再生成される前提。
/// -------------------------------------------------------------
class GamePlayWorld
{
public: /// ---------- メンバ関数 ---------- ///

	// StageContextのステージ情報をもとに、ステージ/キャラクター/衝突/HUD/目的管理を生成する。
	void Initialize(GamePlayStageContext& stageContext);
	// CollisionManager登録を外し、所有するWorld内オブジェクトとグローバル参照を解放する。
	void Finalize();

	// World内のステージ、キャラクター、ボス、弾、当たり判定、HUD、目的条件を1フレーム進める。
	void Update(float deltaTime);

	// イントロ中に表示に必要なステージ・影・空だけを更新する。
	void UpdateIntroVisuals();
	// 武器構えイントロ中に、通常AIは止めたままプレイヤー表示だけを更新する。
	void UpdateEquipIntro(float deltaTime);
	// イントロ終了直後の初回フレーム負荷を避けるため、敵や武器表示を非表示で事前更新する。
	void WarmupStartGameplayForIntro();
	// イントロ用に非表示で温めたプレイヤー/武器/敵の見た目を切り替える。
	void SetStartGameplayVisualsVisible(bool visible);

	// 3D要素を描画する。イントロ中はキャラクターだけを隠して背景確認を優先できる。
	void Draw3D(bool hideCharactersDuringIntro);
	// シャドウ用描画を行う。通常描画と同じイントロ非表示条件を使う。
	void DrawShadow(bool hideCharactersDuringIntro);
	// HUDと敵HPバーを描画する。イントロ中は呼び出し側から隠せる。
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
	bool IsGameClearRequested() const { return isGameClear_; }

	void SetDebugCameraEnabled(bool enabled);

	// 防衛対象の実オブジェクト破壊イベントが接続されるまで、デバッグ/仮実装から失敗条件を反映する窓口。
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

	// 装置オブジェクトの接触イベント実装後は、各装置から呼ばれる想定の進行加算窓口。
	void AddActivatedDeviceCount(int amount = 1);
	// ゴール接触判定が実オブジェクト側へ移るまで、到達フラグを外部から反映する窓口。
	void SetReachedGoal(bool reached);
	// ボス撃破/クリアアイテム取得の結果をステージ目的管理へ反映する。
	void SetBossDefeated(bool defeated);
	// クリスタル破壊イベントから将来のボス登場演出へつなぐための読み取り口。
	bool HasCrystalBroken() const { return crystalManager_.HasCrystalBroken(); }
	bool IsFinalPhaseReady() const { return crystalManager_.IsFinalPhaseReady(); }
	bool IsBossAppearRequested() const { return crystalManager_.IsBossAppearRequested(); }
	bool IsWorldColorChangeComplete() const { return crystalManager_.IsWorldColorChangeComplete(); }

private: /// ---------- メンバ関数 ---------- ///

	void CollisionUpdate();
	void UpdateShadowLightViewProjection();
	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const;
	bool IsSightBlocked(const K4E::Segment& seg) const;
	void UpdateCrystalBossSpawnProgress();
	void SpawnGuardianBoss();
	void UpdateBossClearProgress(float deltaTime);
	void SpawnClearItem(const K4E::Vector3& bossPosition);
	void CollectClearItem();


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
	std::unique_ptr<GuardianBoss> guardianBoss_;
	std::unique_ptr<BossClearItem> clearItem_;

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
	K4E::Vector3 bossSpawnPosition_{ 0.0f, 2.25f, 30.0f };
	bool bossSpawned_ = false;
	bool bossSpawnConditionMet_ = false;
	bool bossDefeated_ = false;
	bool clearItemSpawned_ = false;
	bool clearItemCollected_ = false;
	bool isGameClear_ = false;
};
