#pragma once
#include <Object3D.h>
#include "Vector3.h"
#include "Collider.h"
#include "CollisionTypeIdDef.h"
#include <memory>

/// ---------- 前方宣言 ---------- ///
class Player;

/// -------------------------------------------------------------
///                     　　敵クラス
/// -------------------------------------------------------------
class Enemy : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	~Enemy();

	void Initialize(Player* player, const Vector3& position);

	void Update();

	void Draw();

	void OnCollision(Collider* other) override;

	void TakeDamage(int damage);

	bool IsDead() const { return isDead_; }

	void ApplyKnockback(const Vector3& direction, float power);

private: /// ---------- ヘルパー関数 ---------- ///

	Vector3 PlayerWorldPosition();

	void ApplyMeleeDamage();

	void SeparateFromNeighbors(Vector3& currentPos, float dt);

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // プレイヤーの参照

	std::unique_ptr<Object3D> model_; // モデル

	static std::vector<Enemy*> sActives;   // アクティブ敵の一覧
	float avoidRadius_ = 1.2f;             // 体の半径（OBB半径相当でOK）

	Vector3 knockVel_{}; // ノックバック速度

	bool isDead_ = false;      // 死亡フラグ
	float attackCooldown_ = 1.0f; // 攻撃クールダウン時間
	float attackTimer_ = 0.0f; // 攻撃タイマー

	float maxHp_ = 40.0f;
	float hp_ = 0.0f;
	float speed_ = 5.5f;
	float attackRange_ = 1.6f;
	float dps_ = 12.0f;
	float knockback_ = 2.0f;
};

