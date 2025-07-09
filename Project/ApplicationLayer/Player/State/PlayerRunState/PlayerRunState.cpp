#include "PlayerRunState.h"
#include "Player.h"

#include "PlayerIdleState.h"
#include "PlayerWalkState.h"

void PlayerRunState::Initialize(Player* player)
{
	// 走行アニメーションモデルの初期化
	player->GetAnimationModel()->Initialize(modelFilePath_);
	player->GetAnimationModel()->SetDissolveThreshold(0.0f); // ダメージを受けていないので閾値は0.0f
	player->GetAnimationModel()->Update();
}

void PlayerRunState::Update(Player* player)
{
    // 入力が無くなったら待機へ
    if (Vector3::Length(player->GetMoveInput()) <= 0.0f)
    {
        player->ChangeState(std::make_unique<PlayerIdleState>());
        return;
    }

	// ダッシュ入力が無くなったら歩行へ
	if (!player->GetController()->IsDashing())
	{
		player->ChangeState(std::make_unique<PlayerWalkState>());
		return;
	}
}

void PlayerRunState::Finalize(Player* player)
{
}

void PlayerRunState::Draw(Player* player)
{
}
