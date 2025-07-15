#include "IdleBehavior.h"
#include "Player.h"

void IdleBehavior::Initialize(Player* player)
{
	AnimationModel* model = player->GetAnimationModel();
	model->Initialize("PlayerStateModel/human.gltf");
	model->Update();
	player->SetState(ModelState::Idle); // 初期状態を Idle に設定
}

void IdleBehavior::Update(Player* player)
{
	AnimationModel* model = player->GetAnimationModel();
	model->Update();

	Vector3 move = player->GetMoveInput();
	bool isMoving = std::abs(move.x) > 0.01f || std::abs(move.z) > 0.01f;

	// 移動を検知したとき
	if (isMoving)
	{
		if (player->IsDashing())
		{
			// 走行状態に移行
			player->SetState(ModelState::Running);
			return;
		}
		else
		{
			// 歩行状態に移行
			player->SetState(ModelState::Walking);
			return;
		}
	}

	// ジャンプ入力を検知したとき
	if (player->GetController()->IsJumpTriggered())
	{
		// ジャンプ状態に移行
		player->SetState(ModelState::Jumping);
		return;
	}

	// クラウチング入力を検知したとき
	if (player->IsCrouching())
	{
		// しゃがみ状態に移行
		player->SetState(ModelState::Crouching);
		return;
	}

	// ADS入力を検知したとき
	if (player->IsAiming())
	{
		// エイム状態に移行
		player->SetState(ModelState::Aiming);
		return;
	}

	// ⑤ 射撃中（単発 or 押しっぱなし）
	if (player->GetController()->IsTriggerShooting() || player->GetController()->IsPushShooting())
	{
		player->SetState(ModelState::Shooting);
		return;
	}
}

void IdleBehavior::Draw(Player* player)
{
	player->GetAnimationModel()->Draw();
}