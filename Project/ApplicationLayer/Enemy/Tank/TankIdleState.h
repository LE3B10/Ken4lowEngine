#pragma once
#include "IEnemyState.h"
#include "Vector3.h"

class Player;
class Enemy;

class TankIdleState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// ステートの開始処理
	void Enter(Enemy* enemy) override;
	
	// ステートの更新処理
	void Update(Enemy* enemy) override;
	
	// ステートの終了処理
	void Exit(Enemy* enemy) override;

private: /// ---------- メンバ関数 ---------- ///

	void PlayGroundShakeEffect(); // 地響き演出を再生

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // プレイヤーへのポインタ
	Enemy* enemy_ = nullptr; // エネミーへのポインタ

	float idleTimer_ = 0.0f; // アイドルタイマー
	float deltaTime = 1.0f / 60.0f; // フレーム時間（60FPS想定）
};

