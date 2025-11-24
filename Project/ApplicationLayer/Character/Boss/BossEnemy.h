#pragma once
#include "BaseCharacter.h"
#include "ContactRecord.h"

/// -------------------------------------------------------------
///					　ボス敵キャラクタークラス
/// -------------------------------------------------------------
class BossEnemy : public BaseCharacter
{
public: /// ---------- 列挙型 ---------- ///

	// ボス敵の状態
	enum class State
	{

	};

public: /// ---------- 構造体 ---------- ///

	// 分解運動データ構造体
	struct GibMotion
	{
		Vector3 velocity;		 // 初速度
		Vector3 angularVelocity; // 角速度（ラジアン）
	};

	// 死亡演出状態構造体
	struct DeathEnemyState
	{
		bool  active = false;		 // 死亡演出中
		bool  finished = false;		 // 完了（クリア判定用）
		float timer = 0.0f;			 // 経過時間
		float duration = 1.2f;		 // 分解が終わるまでの時間
		std::vector<GibMotion> gibs; // 各部位の分解運動データ
		GibMotion bodyGib;			 // 体幹部位の分解運動データ
	};

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~BossEnemy() = default;

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突判定を行う
	void OnCollision(Collider* other) override;

	// 中心座標を取得
	Vector3 GetCenterPosition() const override;

public: /// ---------- アクセッサ ---------- ///

	// 死亡状態かどうか取得
	bool IsDeadNow() const { return death_.finished; }

private: /// ---------- メンバ変数 ---------- ///

	DeathEnemyState death_; // 死亡演出状態

	ContactRecord contactRecord_; // 接触記録

	// スキンテクスチャのパス
	const std::string skinTexturePath_ = "zombie.png";
};

