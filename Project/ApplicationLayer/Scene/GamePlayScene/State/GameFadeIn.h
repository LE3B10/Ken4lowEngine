#pragma once
#include "IGamePlaySceneState.h"

/// -------------------------------------------------------------
///			ゲームプレイシーン：フェードイン状態
/// -------------------------------------------------------------
class GameFadeIn : public IGamePlaySceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~GameFadeIn() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Enter(GamePlayScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	void Update(GamePlayScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Exit(GamePlayScene* scene) override;

private: /// ---------- メンバ変数 ---------- ///

	float timer_ = 0.0f; 	 // フェードイン経過時間
	float duration_ = 0.5f; // フェードインにかける時間（秒）
};

