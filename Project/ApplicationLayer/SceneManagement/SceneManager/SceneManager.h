#pragma once
#include <BaseScene.h>
#include <TitleScene.h>
#include <GamePlayScene.h>
#include "AbstractSceneFactory.h"

/// -------------------------------------------------------------
///					　	シーンの管理クラス
/// -------------------------------------------------------------
class SceneManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static SceneManager* GetInstance();

	// 更新処理
	void Update();

	// 3Dオブジェクトの描画
	void Draw3DObjects();

	// 2Dオブジェクトの描画
	void Draw2DSprites();

	// ImGui描画処理
	void DrawImGui();

	// 終了処理
	void Finalize();

public: /// ---------- セッタ ---------- ///

	// 次のシーンの設定
	void SetNextScene(std::unique_ptr<BaseScene> nextScene) { nextScene_ = std::move(nextScene); }

	// シーンファクトリーの設定
	void SetAbstractSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory) { sceneFactory_ = std::move(sceneFactory); }

	// シーン切り替え
	void ChangeScene(const std::string& sceneName);

private: /// ---------- メンバ関数 ---------- ///

	// 今のシーン
	std::unique_ptr<BaseScene> scene_;

	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_;

	// シーンファクトリー
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
};

