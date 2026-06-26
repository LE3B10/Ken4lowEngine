#pragma once
#include <Actor.h>

/// ---------- 前方宣言 ---------- ///

// ActorComponentの動作確認を行うテスト用ActorComponentクラス
class TestActorComponent;

/// -------------------------------------------------------------
///	 Actor・ActorComponent・ActorWorldの動作確認を行うテスト用Actorクラス
/// -------------------------------------------------------------
class TestActor final : public Ken4lowEngine::Actor
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// TestActor生成後の初期化処理を行う。
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// TestActorの1フレーム更新処理を行う。
	/// </summary>
	void Update(float deltaTime) override;

	/// <summary>
	/// TestActorの通常描画処理を行う。
	/// </summary>
	void Draw() override;

	/// <summary>
	/// TestActorのシャドウ描画処理を行う。
	/// </summary>
	void DrawShadow() override;

	/// <summary>
	/// TestActorのImGui描画処理を行う。
	/// </summary>
	void DrawImGui() override;

	/// <summary>
	/// TestActorの終了処理を行う。
	/// </summary>
	void Finalize() override;
};
