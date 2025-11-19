#pragma once
#include "ITitleSceneState.h"

/// -------------------------------------------------------------
///				　	タイトルシーンのロビー状態
/// -------------------------------------------------------------
class TitleLobbyState : public ITitleSceneState
{
public: /// ---------- 仮想関数のオーバーライド ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~TitleLobbyState() override = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Enter(TitleScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Update(TitleScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Exit(TitleScene* scene) override;
};

