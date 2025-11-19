#pragma once
#include "IGamePlaySceneState.h"

/// -------------------------------------------------------------
///				　		ゲームクリア状態
/// -------------------------------------------------------------
class GameClearState : public IGamePlaySceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~GameClearState() = default;

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
};

