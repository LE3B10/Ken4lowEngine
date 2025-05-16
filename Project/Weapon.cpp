#include "Weapon.h"

void Weapon::Initialize(const std::string& modelPath)
{
	object_ = std::make_unique<Object3D>();
	object_->Initialize(modelPath);
}

void Weapon::Update()
{
	if (parentTransform_)
	{
		worldTransform_.translate_ = parentTransform_->worldTranslate_ + offset_;
		worldTransform_.rotate_ = parentTransform_->worldRotate_;
		//worldTransform_.scale_ = parentTransform_->scale_;
	}

	// 武器のワールド変換を更新
	object_->SetTranslate(worldTransform_.translate_);
	object_->SetRotate(worldTransform_.rotate_);
	object_->SetScale(worldTransform_.scale_);

	object_->Update();
}

void Weapon::Draw()
{
	object_->Draw();
}
