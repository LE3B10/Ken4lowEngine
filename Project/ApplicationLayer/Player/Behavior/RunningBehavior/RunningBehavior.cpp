#include "RunningBehavior.h"
#include "Player.h"
#include "Input.h"
#include "AnimationModel.h"
#include <AnimationModelFactory.h>

void RunningBehavior::Initialize(Player* player)
{
	auto model = AnimationModelFactory::CreateInstance("PlayerStateModel/PlayerRunState.gltf");
	player->SetAnimationModel(model);
	model->Update();
	player->SetState(ModelState::Running);
}

void RunningBehavior::Update(Player* player)
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
	if (!player->IsDashing())
	{
		player->SetState(ModelState::Walking);
		return;
	}
}

void RunningBehavior::Draw(Player* player)
{
	player->GetAnimationModel()->Draw();
}
