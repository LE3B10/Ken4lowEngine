#pragma once
#include "BossBullet.h"

#include <memory>
#include <vector>
#include <string>

/// ---------- 前方宣言 ---------- ///
class Boss;

/// ---------------------------------------------------------------
//				　			ボスの武器クラス
/// ---------------------------------------------------------------
class BossWeapon
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update(); // 発射タイミング管理

	// 発射処理
	void TryFire(const Vector3& pos, const Vector3& dir);

	// 描画処理
	void Draw();

public: /// ---------- アクセッサ ---------- ///

	void SetFireInterval(float interval) { fireInterval_ = interval; }
	void SetBulletSpeed(float speed) { bulletSpeed_ = speed; }
	void SetDamage(float damage) { bulletDamage_ = damage; }
	void StartBurstFire(const Vector3& pos, const Vector3& dir);

	// 弾を全て取得
	const std::vector<std::unique_ptr<BossBullet>>& GetBullets() const { return bullets_; }

	// ボスへの参照を設定
	void SetBoss(Boss* boss) { boss_ = boss; }

private: /// ---------- メンバ変数 ---------- ///

	Boss* boss_ = nullptr; // ボスへの参照（必要なら）
	std::vector<std::unique_ptr<BossBullet>> bullets_;

	float fireInterval_ = 0.2f;
	float fireTimer_ = 0.0f;

	float bulletSpeed_ = 40.0f;
	float bulletDamage_ = 25.0f;

	float deltaTime_ = 1.0f / 60.0f; // 更新間隔（例：60FPSなら1/60秒）

	int burstCount_ = 0;             // 残り発射回数
	const int maxBurstCount_ = 3;    // 1回のバーストで撃つ弾数
	float burstInterval_ = 0.001f;     // 1発ごとの間隔
	float burstTimer_ = 0.0f;
	bool isBursting_ = false;

	Vector3 fireDir_ = {};           // 発射方向
	Vector3 firePos_ = {};           // 発射位置
};

