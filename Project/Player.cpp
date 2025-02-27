#include "Player.h"
#include <Object3DCommon.h>
#include <ModelManager.h>


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
    // 体（親）の初期化
    body_.object = std::make_unique<Object3D>();
    body_.object->Initialize("Player/body.gltf");
    body_.transform.translate_ = { 0, 0, 0 };

    // 子オブジェクト（頭、腕）をリストに追加
    std::vector<std::pair<std::string, Vector3>> partData =
    {
        {"Player/Head.gltf", {0, 5.0f, 0}},     // 頭
        {"Player/L_arm.gltf", {0.0f, 4.5f, 0}}, // 左腕
        {"Player/R_arm.gltf", {0.0f, 4.5f, 0}}  // 右腕
    };

    for (const auto& [modelPath, position] : partData)
    {
        BodyPart part;
        part.object = std::make_unique<Object3D>();
        part.object->Initialize(modelPath);
        part.transform.translate_ = position;
		part.object->SetTranslate(part.transform.translate_);
        part.transform.parent_ = &body_.transform; // 親を設定
        parts_.push_back(std::move(part));
    }
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
    // 体を更新
    body_.object->Update();

    // 各部位を更新
    for (auto& part : parts_)
    {
        part.object->Update();
    }
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
    // 体を描画
    body_.object->Draw();

    // 各部位を描画
    for (auto& part : parts_)
    {
        part.object->Draw();
    }
}
