#include "DeadBehavior.h"
#include "Player.h"


void DeadBehavior::Initialize(Player* player)
{
	AnimationModel* animationModel = player->GetAnimationModel();
	animationModel->Initialize("");
	animationModel->Update();
	player->SetState(ModelState::Dead); // Dead状態に設定
}

void DeadBehavior::Update(Player* player)
{
	AnimationModel* animationModel = player->GetAnimationModel();
	animationModel->Update();
}

void DeadBehavior::Draw(Player* player)
{
	player->GetAnimationModel()->Draw(); // アニメーションモデルの描画
}
