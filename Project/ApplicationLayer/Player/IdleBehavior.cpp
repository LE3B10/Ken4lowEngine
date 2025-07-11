#include "IdleBehavior.h"
#include "Player.h"
#include "Input.h"

void IdleBehavior::Initialize(Player* player)
{
	// 初期化処理は特に必要ないが、将来の拡張のために残しておく
	AnimationModel* model = player->GetAnimationModel();
	model->Initialize("PlayerStateModel/human.gltf"); // モデルの初期化
	model->Update(); // アニメーションモデルの更新
	player->SetCurrentState(ModelState::Idle); // 状態をIdleに設定
}

void IdleBehavior::Update(Player* player)
{
	player->GetAnimationModel()->Update(); // アニメーションモデルの更新

	Input* input = player->GetInput();

	// 移動キーの入力をチェック
	bool isMoving = input->PushKey(DIK_W) || input->PushKey(DIK_A) || input->PushKey(DIK_S) || input->PushKey(DIK_D);
	bool isMovingPad = !input->LStickInDeadZone();

	// 走行キーの入力をチェック
	bool isRunning = input->TriggerKey(DIK_LSHIFT);

	// ジャンプキーの入力をチェック
	bool jumpKey = input->TriggerKey(DIK_SPACE);
	bool jumpPad = input->TriggerButton(XButtons.A);

	if (isMoving || isMovingPad)
	{
		player->SetCurrentState(ModelState::Walking);
	}
	else if (isMoving && isRunning)
	{
		player->SetCurrentState(ModelState::Running);
	}
	else if (jumpKey || jumpPad)
	{
		player->SetCurrentState(ModelState::Jumping);
	}
	else if (input->PushKey(DIK_A) || input->PushKey(DIK_D))
	{
		player->SetCurrentState(ModelState::Attacking);
	}
	else
	{
		player->SetCurrentState(ModelState::Idle);
	}
}

void IdleBehavior::Draw(Player* player)
{
	// 描画処理はアニメーションモデルに任せる
	player->GetAnimationModel()->Draw();
}
