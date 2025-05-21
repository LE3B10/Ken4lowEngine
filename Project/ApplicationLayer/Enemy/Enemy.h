#pragma once
#include "BaseCharacter.h"
#include "EnemyBullet.h"

class Player;


class Enemy : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	// 対象を設定
	void SetTarget(Player* player) { player_ = player; }

	// 弾丸を取得
	const std::list<std::unique_ptr<EnemyBullet>>& GetBullets() const { return bullets_; }

private: /// ---------- メンバ関数 ---------- ///

	// プレイヤー専用パーツの初期化
	//void InitializeParts() override;

	// 衝突判定と応答
	void OnCollision(Collider* other) override;

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // 対象プレイヤー

	float shootCooldown_ = 2.0f; // クールダウン間隔（秒）
	float shootTimer_ = 0.0f;

	std::list<std::unique_ptr<EnemyBullet>> bullets_; // 発射した弾リスト
};

