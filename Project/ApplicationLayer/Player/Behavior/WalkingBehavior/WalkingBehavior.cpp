#include "WalkingBehavior.h"
#include "Player.h"
#include "Input.h"
#include "Vector3.h"
//#include <AnimationModelFactory.h>

void WalkingBehavior::Initialize(Player* player)
{
	//auto model = AnimationModelFactory::CreateInstance("PlayerStateModel/humanWalking.gltf");
	//player->SetAnimationModel(model);
	//model->Update();
	//player->SetState(ModelState::Walking);
}

void WalkingBehavior::Update(Player* player)
{
	AnimationModel* model = player->GetAnimationModel();
	model->Update();

	Vector3 move = player->GetMoveInput();
	bool isMoving = std::abs(move.x) > 0.01f || std::abs(move.z) > 0.01f;
	if (!isMoving)
	{
		player->SetState(ModelState::Idle);
		return;
	}
	if (player->IsDashing())
	{
		player->SetState(ModelState::Running);
		return;
	}
}

void WalkingBehavior::Draw(Player* player)
{
	player->GetAnimationModel()->Draw();
}
