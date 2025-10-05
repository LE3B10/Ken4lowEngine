#pragma once
#include <memory>
#include <vector>
#include "Vector3.h"
#include <Object3D.h>

class Player;

/// -------------------------------------------------------------
///                     　　敵弾クラス
/// -------------------------------------------------------------
class EnemyBullet
{
public: /// ---------- メンバ関数 ---------- ///

	static void Create(const Vector3& pos, const Vector3& vel, float damage, float life = 3.0f);

	static void UpdateAll(Player* player, float dt);

	static void DrawAll();

	static void ClearAll();

private:

	EnemyBullet(const Vector3& pos, const Vector3& vel, float dmg, float life);

private: /// ---------- メンバ変数 ---------- ///

	Vector3 pos_;
	Vector3 vel_;
	float   damage_;
	float   life_;
	float   radius_ = 0.6f; // プレイヤー当たり判定用の簡易半径

	std::unique_ptr<Object3D> model_;
	static std::vector<std::unique_ptr<EnemyBullet>> sBullets_;
};
