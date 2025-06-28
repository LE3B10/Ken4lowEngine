#include "PlayerWalkState.h"
#include "Player.h"
#include "PlayerIdleState.h"

void PlayerWalkState::Initialize()
{
	// アニメーションモデルの初期化
	animationModel_ = std::make_unique<AnimationModel>();
	animationModel_->Initialize(modelFilePath_); // スキニング有効
}

void PlayerWalkState::Update()
{
	// アニメーションの更新
	if (animationModel_) animationModel_->Update();

	// 入力処理
	auto input = Input::GetInstance();
	if (input->PushKey(DIK_W) || input->PushKey(DIK_S) || input->PushKey(DIK_A) || input->PushKey(DIK_D)) {
		// 移動入力があれば、歩行状態に遷移
		// ここではプレイヤーの状態遷移を行う必要があります
	}
}

void PlayerWalkState::Draw()
{
	// アニメーションモデルの描画
	if (animationModel_) animationModel_->Draw();
}
