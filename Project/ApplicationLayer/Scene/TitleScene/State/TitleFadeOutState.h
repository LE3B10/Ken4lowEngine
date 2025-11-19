#pragma once
#include "ITitleSceneState.h"

/// -------------------------------------------------------------
///				タイトルシーンのフェードアウト状態
/// -------------------------------------------------------------
class TitleFadeOutState : public ITitleSceneState
{
public: /// ---------- 仮想関数のオーバーライド ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~TitleFadeOutState() override = default;

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

private: /// ---------- メンバ変数 ---------- ///

	float timer_ = 0.0f; // フェード用タイマー
	float duration_ = 0.5f;  // フェードにかける時間（秒）
};

