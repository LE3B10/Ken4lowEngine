#pragma once
#include "IGamePlaySceneState.h"

/// -------------------------------------------------------------
///				　ゲームプレイシーンのロード状態
/// -------------------------------------------------------------
class GameLoadState : public IGamePlaySceneState
{
public: /// ---------- 仮想関数のオーバーライド ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	~GameLoadState() override = default;

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

	void Draw3DObjects(GamePlayScene* scene) override;

	void Draw2DSprites(GamePlayScene* scene) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Exit(GamePlayScene* scene) override;

private: /// ---------- メンバ変数 ---------- ///

	float timer_ = 0.0f; 	 // ロード経過時間
	float duration_ = 1.0f; // ロードにかける時間（秒）
};

