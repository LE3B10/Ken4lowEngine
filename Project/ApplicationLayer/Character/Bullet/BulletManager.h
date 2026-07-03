#pragma once
#include "Bullet.h"
#include "WeaponParams.h"

#include <memory>
#include <vector>
#include <functional>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;
namespace Ken4lowEngine { class PhysicsWorld; }

/// -------------------------------------------------------------
///                     弾管理クラス
/// -------------------------------------------------------------
class BulletManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(CollisionManager* collisionManager);

	// 生成（dirは正規化済み推奨。speedは units/sec）
	// typeId: CollisionTypeIdDef の弾種（デフォはプレイヤー弾）
	Bullet* Spawn(const Ken4lowEngine::Vector3& startPos,
		const Ken4lowEngine::Vector3& dir,
		float speed,
		int damage = 1,
		float lifeTimeSec = 3.0f,
		const Ken4lowEngine::Vector3& shooterPosition = { 0.0f, 0.0f, 0.0f },
		uint32_t shooterColliderId = 0u,
		uint32_t typeId = static_cast<uint32_t>(CollisionTypeIdDef::kBullet),
		const WeaponParams& weaponParams = WeaponParams()
	);

	// 更新処理
	void Update(float dt);

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	// クリア処理
	void Clear();

	// プレイヤー弾のWorld着弾演出をGamePlayScene側へ通知する。
	void SetWorldImpactCallback(std::function<void(const Ken4lowEngine::Vector3&, const Ken4lowEngine::Vector3&)> callback);

	// 通常弾だけをPhysicsWorld Triggerへ段階移行するため、登録先Worldとレイヤーを設定する。
	void SetPhysicsTriggerWorld(Ken4lowEngine::PhysicsWorld* physicsWorld, uint32_t playerBulletLayer);
	void SetUsePhysicsTriggerForNormalBullets(bool enabled);
	void RefreshPhysicsTriggerRegistrations();

public: /// ---------- アクセサ ---------- ///

	size_t GetCount() const { return bullets_.size(); }
	size_t GetActiveCount() const;
	size_t GetPhysicsTriggerBulletCount() const;
	int GetPhysicsTriggerHitCount() const { return physicsTriggerHitCount_; }

private: /// ---------- メンバ変数 ---------- ///

	// 衝突管理マネージャー（弾の衝突判定用）
	CollisionManager* collisionManager_ = nullptr;
	Ken4lowEngine::PhysicsWorld* physicsWorld_ = nullptr;
	uint32_t playerBulletLayer_ = 0u;
	bool usePhysicsTriggerForNormalBullets_ = false;
	int physicsTriggerHitCount_ = 0;
	std::function<void(const Ken4lowEngine::Vector3&, const Ken4lowEngine::Vector3&)> worldImpactCallback_{};

	// 弾リスト
	std::vector<std::unique_ptr<Bullet>> bullets_;
};
