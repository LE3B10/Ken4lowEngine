#include "PlayerIdleState.h"
#include <Input.h>

void PlayerIdleState::Initialize()
{
	// アニメーションモデルの初期化
	animationModel_ = std::make_unique<AnimationModel>();
	animationModel_->Initialize(modelFilePath_); // スキニング有効
}

void PlayerIdleState::Update()
{
	// アニメーションの更新
	if (animationModel_) {
		animationModel_->Update();
	}
	// 入力処理
	auto input = Input::GetInstance();
	if (input->PushKey(DIK_W) || input->PushKey(DIK_S) || input->PushKey(DIK_A) || input->PushKey(DIK_D)) {
		// 移動入力があれば、歩行状態に遷移
		// ここではプレイヤーの状態遷移を行う必要があります
	}
}

void PlayerIdleState::Draw()
{
	if (animationModel_) {
		// アニメーションモデルの描画
		animationModel_->Draw();
	}
}
