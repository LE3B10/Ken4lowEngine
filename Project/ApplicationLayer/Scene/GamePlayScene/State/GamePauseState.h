#pragma once
#include "IGamePlaySceneState.h"

/// -------------------------------------------------------------
///				　ゲームプレイシーンのポーズ状態
/// -------------------------------------------------------------
class GamePauseState : public IGamePlaySceneState
{
public: /// ---------- 仮想関数のオーバーライド ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~GamePauseState() override = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Enter(GamePlayScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Update(GamePlayScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Exit(GamePlayScene* scene) override;
};

