#pragma once
#include "IStageSelectSceneState.h"

/// -------------------------------------------------------------
///			ステージセレクトシーン：フェードイン状態
/// -------------------------------------------------------------
class StageSelectFadeInState : public IStageSelectSceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~StageSelectFadeInState() = default;

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

	float timer_ = 0.0f; 	 // フェードイン経過時間
	float duration_ = 0.5f; // フェードインにかける時間（秒）
};

