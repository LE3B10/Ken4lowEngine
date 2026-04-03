#pragma once
#include <BaseScene.h>
#include "AbstractSceneFactory.h"

#include "FadeManager.h"

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

	// シャドウマップ用オブジェクトの描画
	void DrawShadowObjects();

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

	bool IsTransitioning() const { return isTransitioning_ || (fadeManager_ && fadeManager_->IsBusy()); }

private: /// ---------- メンバ関数 ---------- ///

	// 次シーン適用（Finalize→差し替え→Initialize）
	void ApplyNextScene();

private: /// ---------- メンバ変数 ---------- ///

	// 現在のシーン
	std::unique_ptr<BaseScene> scene_;

	// 次のシーン
	std::unique_ptr<BaseScene> nextScene_;

	// シーンファクトリー
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

	// フェードマネージャー
	std::unique_ptr<FadeManager> fadeManager_;
	bool isTransitioning_ = false; // フェード遷移中
	bool sceneSwapped_ = false; // 既にシーン差し替え済みか
	bool pendingCrack_ = false; // 差し替え直後、次フレームでCrackを開始

	bool hasQueuedChange_ = false;
	std::string queuedSceneName_;

	// カバー完了後に少し待ってから差し替える
	int coverHoldFrames_ = 4;
	int coverHoldCounter_ = 0;

	// 差し替え後、少し待ってから Crack を始める
	int uncoverDelayFrames_ = 3;
	int uncoverDelayCounter_ = 0;

	bool unloadRequested_ = false; // アンロード要求フラグ
};
