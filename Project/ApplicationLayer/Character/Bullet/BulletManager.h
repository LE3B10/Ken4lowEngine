#pragma once
#include "Bullet.h"

#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;

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
		float splashRadius = 0.0f,
		int splashDamage = 0,
		bool splashCanDamageSelf = false,
		bool drawModel = true
		);

	// 更新処理
	void Update(float dt);

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	// クリア処理
	void Clear();

public: /// ---------- アクセサ ---------- ///

	size_t GetCount() const { return bullets_.size(); }
	size_t GetActiveCount() const;

private: /// ---------- メンバ変数 ---------- ///

	// 衝突管理マネージャー（弾の衝突判定用）
	CollisionManager* collisionManager_ = nullptr;

	// 弾リスト
	std::vector<std::unique_ptr<Bullet>> bullets_;
};

