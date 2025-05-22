#pragma once
#include "BaseCharacter.h"
#include "EnemyBullet.h"
#include "IEnemyState.h"
#include "ContactRecord.h"

#include <memory>

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

	// ステートを変更
	void ChangeState(std::unique_ptr<IEnemyState> newState);

	// リクエスト発射
	void RequestShoot(); // 弾発射要求（AttackStateから呼ぶ）

	// 対象を設定
	void SetTarget(Player* player) { player_ = player; }

	// 座標
	void SetTranslate(const Vector3& translate) { body_.worldTransform_.translate_ = translate; }

	// 向き
	void SetRotate(const Vector3& rotate) { body_.worldTransform_.rotate_ = rotate; }

	// Getter/Setter
	const Vector3& GetIdleBasePosition() const { return idleBasePosition_; }
	void SetIdleBasePosition(const Vector3& pos) { idleBasePosition_ = pos; }

	const Vector3& GetIdleMoveDirection() const { return idleMoveDirection_; }
	void SetIdleMoveDirection(const Vector3& dir) { idleMoveDirection_ = dir; }

	// 
	void SetStateName(const std::string& name) { currentStateName_ = name; }

	// 弾丸を取得
	const std::list<std::unique_ptr<EnemyBullet>>& GetBullets() const { return bullets_; }

	Player* GetTargetPlayer() const { return player_; } // プレイヤー参照

	// 向きを取得
	float GetYaw() const { return body_.worldTransform_.rotate_.y; }

	// 回転を取得
	Vector3 GetRotate() const { return body_.worldTransform_.rotate_; }

private: /// ---------- メンバ関数 ---------- ///

	// プレイヤー専用パーツの初期化
	//void InitializeParts() override;

	// 衝突判定と応答
	void OnCollision(Collider* other) override;

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // 対象プレイヤー

	ContactRecord contactRecord_; // 接触記録

	float shootCooldown_ = 0.1f; // クールダウン間隔（秒）
	float shootTimer_ = 0.0f;

	std::list<std::unique_ptr<EnemyBullet>> bullets_; // 発射した弾リスト

	std::unique_ptr<IEnemyState> currentState_; // 現在の状態

	std::string currentStateName_ = "None";
	// Idle用データ
	Vector3 idleBasePosition_ = {};
	Vector3 idleMoveDirection_ = { 1.0f, 0.0f, 0.0f };

};

