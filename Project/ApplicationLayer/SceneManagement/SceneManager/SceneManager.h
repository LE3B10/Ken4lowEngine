#pragma once
#include <BaseScene.h>
#include "AbstractSceneFactory.h"
#include "FadeManager.h"
#include "Sprite.h"

#include <memory>
#include <string>

/// -------------------------------------------------------------
///					　	シーンの管理クラス
/// -------------------------------------------------------------
class SceneManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static SceneManager* GetInstance();

	// デストラクタを宣言
	~SceneManager();

	void Initialize();

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

	// 画面最前面にフェードだけ描く（遷移中のみ）
	void DrawTransitionOverlay();

public: /// ---------- セッタ ---------- ///

	// 次のシーンの設定
	void SetNextScene(std::unique_ptr<BaseScene> nextScene) { nextScene_ = std::move(nextScene); }

	// シーンファクトリーの設定
	void SetAbstractSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory) { sceneFactory_ = std::move(sceneFactory); }

	// シーン切り替え
	void ChangeScene(const std::string& sceneName, bool useFade = true);

	// 今遷移中か
	bool IsTransitioning() const { return fadeManager_.IsTransitioning(); }

	// フェード時間設定（フレーム数）
	void SetFadeFrames(int frames);

private: /// ---------- メンバ関数 ---------- ///

	// 次シーン適用（Finalize→差し替え→Initialize）
	void ApplyNextScene();

	// 予約している sceneName から nextScene_ を生成（成功したら true）
	bool CreateReservedNextScene();

	// 「開けて良いか？」判定（デフォルトは true。重いシーンだけ待つ）
	bool IsSceneReadyForUncover() const;

private: /// ---------- メンバ関数 ---------- ///

	// 現在のシーン
	std::unique_ptr<BaseScene> scene_;

	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_;

	// シーンファクトリー
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

	// タイルフェード
	FadeManager fadeManager_;

	// ---- 遷移用（CreateSceneを遅延） ----
	bool hasReservedScene_ = false;
	std::string reservedSceneName_;

	// ---- Cover完了後、Uncover開始を待つ ----
	bool waitingUncover_ = false;
};

