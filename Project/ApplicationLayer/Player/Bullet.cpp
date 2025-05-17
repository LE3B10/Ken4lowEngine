#include "Bullet.h"

void Bullet::Initialize()
{
	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetScale({ 0.5f,0.5f,0.5f });
}

void Bullet::Update()
{
	// 位置更新
	position_ += velocity_;

	// 寿命を減らす
	lifeTime_ -= 1.0f / 60.0f;

	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();
}

void Bullet::Draw()
{
	model_->Draw();
}
