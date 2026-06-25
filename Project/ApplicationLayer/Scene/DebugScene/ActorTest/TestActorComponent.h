#pragma once

#include <ActorComponent.h>

/// -------------------------------------------------------------
/// ActorComponentの動作確認を行うテスト用Componentクラス。
/// -------------------------------------------------------------
class TestActorComponent final : public Ken4lowEngine::ActorComponent
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// Component生成後の初期化処理を行う。
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// Componentの1フレーム更新処理を行う。
	/// </summary>
	void Update(float deltaTime) override;

	/// <summary>
	/// Componentの通常描画処理を行う。
	/// </summary>
	void Draw() override;

	/// <summary>
	/// Componentのシャドウ描画処理を行う。
	/// </summary>
	void DrawShadow() override;

	/// <summary>
	/// ComponentのImGui描画処理を行う。
	/// </summary>
	void DrawImGui() override;

	/// <summary>
	/// Componentの終了処理を行う。
	/// </summary>
	void Finalize() override;

private: /// ---------- メンバ変数 ---------- ///

	float elapsedTime_ = 0.0f; // Updateが呼ばれているか確認するための経過時間。
};