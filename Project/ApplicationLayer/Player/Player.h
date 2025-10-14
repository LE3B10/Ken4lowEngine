#pragma once
#include <BaseCharacter.h>
#include <PlayerController.h>

#include <Object3D.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Input;

/// ---------- モデルの状態を表す列挙型 ---------- ///
enum class ModelState
{
	Idle,       // アイドル状態
	Walking,    // 歩行状態
	Running,    // 走行状態
	Jumping,    // ジャンプ状態
	Crouching,  // しゃがみ状態
	Shooting,   // 射撃状態
	Aiming,     // エイム
	Reloading,  // リロード状態
	Dead        // 死亡状態
};


/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Player();

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override {};

private: /// ---------- デバッグカメラフラグ ---------- ///

	Input* input_ = nullptr; // 入力クラス

	std::unique_ptr<Object3D> playerModel_; // プレイヤーモデル
	std::unique_ptr<PlayerController> playerController_; // プレイヤーコントローラー

	bool isDebugCamera_ = false; // デバッグカメラフラグ
};

