#include "PlayerIdleState.h"
#include <Input.h>
#include "Player.h"

#include "PlayerWalkState.h"
#include "PlayerRunState.h"

void PlayerIdleState::Initialize(Player* player)
{
	// 待機アニメに切替え
	player->GetAnimationModel()->Initialize(modelFilePath_);
}

void PlayerIdleState::Update(Player* player)
{
    // 移動入力が入ったら歩行へ
    if (Vector3::Length(player->GetMoveInput()) > 0.0f)
    {
        player->ChangeState(std::make_unique<PlayerWalkState>());
        return;
    }

	// ダッシュ入力があれば走行へ
	if (player->GetController()->IsDashing() && Vector3::Length(player->GetMoveInput()) > 0.0f && 
		player->GetController()->GetCurrentStamina() > player->GetController()->GetStaminaDashCost() * player->GetDeltaTime())
	{
		player->ChangeState(std::make_unique<PlayerRunState>());
		return;
	}
}

void PlayerIdleState::Finalize(Player* player)
{
}

void PlayerIdleState::Draw(Player* player)
{
	
}
