#include "DummyPlayer.h"
#include <Input.h>
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

void DummyPlayer::Initialize()
{
	input_ = K4E::Input::GetInstance();

	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	Collider::SetOwner(this);

	model_ = std::make_unique<K4E::Object3D>();
	model_->Initialize("cube.gltf");
	Collider::SetOBBHalfSize(model_->GetScale());

	debugColor_ = { 0.0f, 1.0f, 0.0f, 1.0f };
	model_->SetColor(debugColor_);
	model_->Update();

	contactRecord_.Clear();
}

void DummyPlayer::Update()
{
	// 移動処理（WASD）
	moveVelocity_ = { 0.0f, 0.0f, 0.0f };
	if (input_->PushKey(DIK_W)) { moveVelocity_.z += 0.1f; }
	if (input_->PushKey(DIK_S)) { moveVelocity_.z -= 0.1f; }
	if (input_->PushKey(DIK_A)) { moveVelocity_.x -= 0.1f; }
	if (input_->PushKey(DIK_D)) { moveVelocity_.x += 0.1f; }

	model_->SetTranslate(K4E::Vector3::Add(model_->GetTranslate(), moveVelocity_));
	Collider::SetCenterPosition(model_->GetTranslate());

	model_->Update();
}

void DummyPlayer::Draw()
{
	if (model_) model_->Draw();
}

void DummyPlayer::DrawImGui()
{
	if (model_) model_->DrawImGui();
}

void DummyPlayer::OnCollisionEnter(K4E::Collider* other)
{
	if (!other) return;

	// エネミー/ボス/弾など、何かに触れたら赤（デバッグ用）
	contactRecord_.Add(other->GetUniqueID());

	debugColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };
	if (model_)
	{
		model_->SetColor(debugColor_);
		model_->Update();
	}
}

void DummyPlayer::OnCollisionExit(K4E::Collider* other)
{
	if (!other) return;

	contactRecord_.Remove(other->GetUniqueID());

	// 接触が無くなったら元の色に戻す
	if (contactRecord_.GetRecordCount() == 0)
	{
		debugColor_ = { 0.0f, 1.0f, 0.0f, 1.0f };
		if (model_)
		{
			model_->SetColor(debugColor_);
			model_->Update();
		}
	}
}
