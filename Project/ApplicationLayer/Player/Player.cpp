#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include <AudioManager.h>
#include <PostEffectManager.h>

#include <imgui.h>


/// -------------------------------------------------------------
///				　			　 デストラクタ
/// -------------------------------------------------------------
Player::~Player()
{

}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));

	playerModel_ = std::make_unique<Object3D>();
	playerModel_->Initialize("player.gltf");
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	playerModel_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	playerModel_->Draw();
}


/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{

}
