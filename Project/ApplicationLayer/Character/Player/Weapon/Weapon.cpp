#include "Weapon.h"

void Weapon::Initialize()
{
	object_ = std::make_unique<Object3D>();
	object_->Initialize("cube.gltf");
}

void Weapon::Update()
{
	object_->Update();
}

void Weapon::Draw()
{
	object_->Draw();
}