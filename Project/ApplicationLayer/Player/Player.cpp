#include "Player.h"
#include "Input.h"

#include "IdleBehavior.h"
#include "WalkingBehavior.h"
#include "RunningBehavior.h"
#include "JumpingBehavior.h"

#include <imgui.h>

void Player::Initialize()
{
	input_ = Input::GetInstance();

	// アニメーションモデルの初期化
	animationModel_ = std::make_unique<AnimationModel>();
	animationModel_->Initialize("PlayerStateModel/human.gltf");
	currentState_ = ModelState::Idle; // 初期状態をIdleに設定

	// 各ビヘイビア登録
	behaviors_[ModelState::Idle] = std::make_unique<IdleBehavior>();
	behaviors_[ModelState::Walking] = std::make_unique<WalkingBehavior>();
	behaviors_[ModelState::Running] = std::make_unique<RunningBehavior>();
	behaviors_[ModelState::Jumping] = std::make_unique<JumpingBehavior>();
}

void Player::Update()
{
	if (input_->TriggerKey(DIK_F2))
	{
		isModelVisible_ = !isModelVisible_;
	}

	// アニメーションモデルの更新
	if (behaviors_.count(currentState_)) {
		behaviors_[currentState_]->Update(this);
	}
}

void Player::Draw()
{
	if (behaviors_.count(currentState_)) {
		if (isModelVisible_) behaviors_[currentState_]->Draw(this);
	}
}

void Player::DrawImGui()
{
	// モデルの描画を有効にするかどうか
	ImGui::Checkbox("Draw Model", &isModelVisible_);
}

void Player::SetCurrentState(ModelState state)
{
	// ステートが変わった時だけ Initialize を呼ぶ
	if (currentState_ != state)
	{
		currentState_ = state;

		if (behaviors_.count(state)) {
			behaviors_[state]->Initialize(this); // 初期化を呼ぶ
		}
	}
}
