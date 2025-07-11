#pragma once
#include "PlayerBehavior.h"
#include "AnimationModel.h"

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

/// ---------- 前方宣言 ---------- ///
class Input;

/// ---------- モデルの状態を表す列挙型 ---------- ///
enum class ModelState
{
	Idle,	   // 待機状態
	Walking,   // 歩行状態
	Running,   // 走行状態
	Jumping,   // ジャンプ状態
	Falling,   // 落下状態
	Attacking, // 攻撃状態
	Dying,     // 死亡状態
};

/// -------------------------------------------------------------
///				　プレイヤークラス
/// -------------------------------------------------------------
class Player
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

public: /// ---------- ゲッタ ---------- ///

	// 入力管理クラスへのポインタを取得
	Input* GetInput() const { return input_; }

	// モデルの状態を取得
	ModelState GetCurrentState() const { return currentState_; }

	// アニメーションモデルを取得
	AnimationModel* GetAnimationModel() const { return animationModel_.get(); }

public: /// ---------- セッタ ---------- ///

	// モデルの状態を設定
	void SetCurrentState(ModelState state);

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力管理クラスへのポインタ

	std::unique_ptr<AnimationModel> animationModel_; // アニメーションモデル
	ModelState currentState_ = ModelState::Idle; // 現在のプレイヤー状態
	bool isDebugCamera_ = false; // デバッグカメラフラグ

	std::unordered_map<ModelState, std::unique_ptr<PlayerBehavior>> behaviors_;

	// モデル描画を有効にするかどうか
	bool isModelVisible_ = true;
};

