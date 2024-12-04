#pragma once
#include <BaseScene.h>
#include <TitleScene.h>
#include <GamePlayScene.h>

/// -------------------------------------------------------------
///					　	シーンの管理クラス
/// -------------------------------------------------------------
class SceneManager
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~SceneManager();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	void DrawImGui();

public: /// ---------- セッタ ---------- ///

	// 次のシーンの設定
	void SetNextScene(std::unique_ptr<BaseScene> nextScene) { nextScene_ = std::move(nextScene); }

private: /// ---------- メンバ関数 ---------- ///

	// 今のシーン
	std::unique_ptr<BaseScene> scene_;

	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_;

};

