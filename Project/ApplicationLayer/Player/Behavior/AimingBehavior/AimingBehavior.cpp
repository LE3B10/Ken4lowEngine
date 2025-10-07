#include "AimingBehavior.h"
#include "Player.h"

void AimingBehavior::Initialize(Player* player)
{
	AnimationModel* animationModel = player->GetAnimationModel();
	animationModel->Initialize("");
	animationModel->Update();
	player->SetState(ModelState::Aiming); // Aiming状態に設定
}

void AimingBehavior::Update(Player* player)
{
	AnimationModel* animationModel = player->GetAnimationModel();
	animationModel->Update();
}

void AimingBehavior::Draw(Player* player)
{
	player->GetAnimationModel()->Draw(); // アニメーションモデルの描画
}
