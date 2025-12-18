#include "RootCageAttack.h"

void RootCageAttack::Initialize()
{
}

void RootCageAttack::TickCooldown(float deltaTime)
{
	(void)deltaTime;
}

bool RootCageAttack::CanAttack() const
{
	return false;
}

void RootCageAttack::Attack()
{
}

void RootCageAttack::Update(Boss* boss, float deltaTime, float bossYawRad, const Vector3& playerPosition)
{
	(void)boss;
	(void)deltaTime;
	(void)bossYawRad;
	(void)playerPosition;
}

bool RootCageAttack::IsActive() const
{
	return false;
}

void RootCageAttack::Draw()
{
}

void RootCageAttack::DrawImGui(Boss& boss)
{
	(void)boss;
}
