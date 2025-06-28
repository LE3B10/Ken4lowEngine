#pragma once
#include "AnimationModel.h"

/// -------------------------------------------------------------
///			プレイヤーのアイドル状態を表すクラス
/// -------------------------------------------------------------
class PlayerIdleState
{
public: /// ---------- メンバ関数 ---------- ///

	// ステートに入った瞬間の処理（アニメ再生、初期化など）
	void Initialize();

	//　毎フレーム呼ばれる更新処理（入力や遷移処理など）
	void Update();

	// 状態に応じた追加描画（HUD変更、特殊エフェクトなど）
	void Draw();

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<AnimationModel> animationModel_; // アニメーションモデル

	// モデルファイルパス
	const std::string modelFilePath_ = "PlayerStateModel/PlayerIdleAnimation.gltf"; // モデルファイルパス
};

