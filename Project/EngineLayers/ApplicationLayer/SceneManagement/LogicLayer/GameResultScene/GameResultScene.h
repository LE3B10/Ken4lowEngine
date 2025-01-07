#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <WavLoader.h>
#include <SRVManager.h>
#include "Input.h"
#include <BaseScene.h>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class ImGuiManager;


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
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager = nullptr;
    Input* input = nullptr;

    std::unique_ptr<WavLoader> wavLoader_;

    std::vector<std::unique_ptr<Sprite>> sprites_;

    // テクスチャのパスをリストで管理
    std::vector<std::string> texturePaths_;
};