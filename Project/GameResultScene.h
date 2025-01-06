#pragma once
#include "BaseScene.h"
#include <Input.h>


/// -------------------------------------------------------------
///                     ゲーム結果シーン
/// -------------------------------------------------------------
class GameResultScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

    // 初期化
    void Initialize() override;

    // 更新処理
    void Update() override;

    // 描画処理
    void Draw() override;

    // 終了処理
    void Finalize() override;

    void DrawImGui() override;

private:

    Input* input = nullptr;
};