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

public: /// ---------- セッタ ---------- ///

	// 次のシーンの設定
	void SetNextScene(BaseScene* nextScene) { nextScene_ = nextScene; }

private: /// ---------- メンバ関数 ---------- ///

	// 今のシーン（実行中のシーン）
	BaseScene* scene_ = nullptr;

	// 次のシーン
	BaseScene* nextScene_ = nullptr;

};

