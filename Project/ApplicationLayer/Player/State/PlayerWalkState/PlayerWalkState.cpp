#include "PlayerWalkState.h"
#include "Player.h"

#include "PlayerIdleState.h"
#include "PlayerRunState.h"

void PlayerWalkState::Initialize(Player* player)
{
	player->GetAnimationModel()->Initialize(modelFilePath_); // 歩行アニメ
}

void PlayerWalkState::Update(Player* player)
{
    // 入力が無くなったら待機へ
    if (Vector3::Length(player->GetMoveInput()) <= 0.0f)
    {
        player->ChangeState(std::make_unique<PlayerIdleState>());
        return;
    }

	// ダッシュ入力があれば走行へ
	if (player->GetController()->IsDashing())
	{
		player->ChangeState(std::make_unique<PlayerRunState>());
		return;
	}
}

void PlayerWalkState::Finalize(Player* player)
{
}

void PlayerWalkState::Draw(Player* player)
{
	
}
