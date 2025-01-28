#include "Player.h"
#include "Object3DCommon.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon)
{
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize(object3DCommon, "cube.gltf");
	object3D_->SetTranslate({ 0.0f,0.0f,0.0f });
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	object3D_->Update();
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	object3D_->Draw();
}
