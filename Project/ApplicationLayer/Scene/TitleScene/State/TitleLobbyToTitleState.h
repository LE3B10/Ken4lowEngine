#pragma once
#include "ITitleSceneState.h"

/// -------------------------------------------------------------
///			 タイトルシーンのロビーからタイトル状態
/// -------------------------------------------------------------
class TitleLobbyToTitleState : public ITitleSceneState
{
public: /// ---------- 仮想関数のオーバーライド ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~TitleLobbyToTitleState() override = default;
	
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

