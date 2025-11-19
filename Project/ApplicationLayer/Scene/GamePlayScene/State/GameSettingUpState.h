#pragma once
#include "IGamePlaySceneState.h"

/// -------------------------------------------------------------
///				ゲームプレイシーン：セッティングアップ状態
/// -------------------------------------------------------------
class GameSettingUpState : public IGamePlaySceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~GameSettingUpState() = default;

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

