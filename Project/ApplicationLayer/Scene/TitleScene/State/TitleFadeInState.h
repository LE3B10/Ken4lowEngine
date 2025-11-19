#pragma once
#include "ITitleSceneState.h"

/// -------------------------------------------------------------
///				　	タイトルシーン：フェードイン状態
/// -------------------------------------------------------------
class TitleFadeInState : public ITitleSceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~TitleFadeInState() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Enter(TitleScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	void Update(TitleScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Exit(TitleScene* scene) override;

private: /// ---------- メンバ変数 ---------- ///

	float timer_ = 0.0f; 	 // フェードイン経過時間
	float duration_ = 0.5f; // フェードインにかける時間（秒）
};

