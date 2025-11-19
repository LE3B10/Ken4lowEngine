#pragma once
#include "IStageSelectSceneState.h"

/// -------------------------------------------------------------
///			ステージセレクトシーン：フェードアウト状態
/// -------------------------------------------------------------
class StageSelectFadeOutState : public IStageSelectSceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~StageSelectFadeOutState() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Enter(StageSelectScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	void Update(StageSelectScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Exit(StageSelectScene* scene) override;

private: /// ---------- メンバ変数 ---------- ///

	float timer_ = 0.0f; 	 // フェードアウト経過時間
	float duration_ = 0.5f; // フェードアウトにかける時間（秒）
};

