#pragma once
#include <Object3D.h>
#include "Vector3.h"

#include "ItemDropTable.h"
#include "Collider.h"
#include "CollisionTypeIdDef.h"
#include <memory>

/// ---------- 前方宣言 ---------- ///
class Player;
class ItemManager;

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

	void TakeDamage(float damage);

	bool IsDead() const { return isDead_; }

	void ApplyKnockback(const Vector3& direction, float power);

	void SetItemManager(ItemManager* itemManager) { itemManager_ = itemManager; }

	// ドロップ処理が完了したか（消滅アニメーションなどが終わったか）
	bool HasDropped() const { return dropProcessed_; }

	void MarkDropped() { dropProcessed_ = true; }
	
	// ドロップ位置を返すヘルパ（モデル座標など）
	Vector3 GetWorldPosition() const;  // 実装は model_->GetTranslate() を返すだけ

private: /// ---------- ヘルパー関数 ---------- ///

	Vector3 PlayerWorldPosition();

	void ApplyMeleeDamage();

	void SeparateFromNeighbors(Vector3& currentPos, float dt);

	void TryRangedAttack(float dist, float dt);

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // プレイヤーの参照
	ItemManager* itemManager_ = nullptr; // アイテムマネージャーの参照
	ItemDropTable itemDropTable_; // アイテムドロップテーブル

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

	// 射撃用パラメータ
	float fireRange_ = 30.0f;  // この距離以内なら撃つ
	float fireDamage_ = 8.0f;   // 1発ダメージ
	float fireCooldown_ = 1.0f;   // 連射間隔（秒）
	float fireTimer_ = 0.0f;   // クールダウン用

	// --- スポーン演出＆停止 ---
	bool  spawning_ = true;        // せり上がり＆硬直中か
	float spawnRiseSpeed_ = 6.0f;  // 上昇速度
	float spawnRiseDepth_ = 1.25f; // 目標Yからどれだけ下から出すか
	float spawnHoldTime_ = 0.35f; // 上がり切ってからの静止時間
	float spawnHoldTimer_ = 0.0f;  // 残り静止時間
	float targetGroundY_ = 1.0f;  // 接地時のY（半分沈まないよう“半径”分）
	float halfHeight_ = 1.0f;  // モデルの半高さ（=OBBのY半径）

	// 召喚を時間で制御（イージング）
	float spawnDuration_ = 1.2f; // 何秒かけて地面下→地面上へ
	float spawnTimer_ = 0.0f;
	bool  spawnEase_ = true; // smoothstepで滑らかに
	float spawnStartY_ = 0.0f; // 下端Y
	float spawnEndY_ = 0.0f; // 上端Y(= targetGroundY_)

	// --- 近接しないための“間合い維持” ---
	float keepMinDist_ = 12.0f; // これより近いと後退
	float keepMaxDist_ = 22.0f; // これより遠いと前進（射程に入れる）

	// 近接ダメージを無効化したい場合
	bool  enableMelee_ = false;  // 近接DPSを切る（必要ならtrue）

	bool dropProcessed_ = false;
};

