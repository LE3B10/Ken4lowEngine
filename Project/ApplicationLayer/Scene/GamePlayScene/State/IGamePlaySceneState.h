#pragma once

/// ---------- 前方宣言 ---------- ///
class GamePlayScene;

/// -------------------------------------------------------------
///				　	ゲームプレイシーンの状態基底クラス
/// -------------------------------------------------------------
class IGamePlaySceneState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~IGamePlaySceneState() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Enter(GamePlayScene* scene) = 0;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Update(GamePlayScene* scene, float deltaTime) = 0;

	/// <summary>
	/// 3Dオブジェクト描画処理（必要に応じてオーバーライド）
	/// </summary>
	/// <param name="scene"></param>
	virtual void Draw3DObjects(GamePlayScene* scene) = 0;

	/// <summary>
	/// スプライト描画処理（必要に応じてオーバーライド）
	/// </summary>
	/// <param name="scene"></param>
	virtual void Draw2DSprites(GamePlayScene* scene) = 0;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Exit(GamePlayScene* scene) = 0;
};

