#pragma once
#include <string>

/// ---------- 前方宣言 ---------- ///
class SceneManager;

/// -------------------------------------------------------------
///                     シーンの基底クラス
/// -------------------------------------------------------------
class BaseScene
{
public:
    virtual ~BaseScene() = default;

    // 仮想初期化処理
    virtual void Initialize() { nextScene_.clear(); } // 次のシーンをリセット

    // 仮想更新処理
    virtual void Update() = 0;

    // 仮想描画処理
    virtual void Draw() = 0;

    // 仮想終了処理（必要であればオーバーライド）
    virtual void Finalize() {}

    // デバッグ用UI描画（必要な場合のみオーバーライド）
    virtual void DrawImGui() {}

    // シーンマネージャーを設定
    virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

    // 次のシーン名を設定
    void SetNextScene(const std::string& sceneName) { nextScene_ = sceneName; }

    // 次のシーン名を取得
    const std::string& GetNextScene() const { return nextScene_; }

protected: /// ---------- メンバ変数 ---------- ///

    SceneManager* sceneManager_ = nullptr;
    std::string nextScene_; // 次のシーン名
};