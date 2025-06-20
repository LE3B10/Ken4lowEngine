#include "Item.h"
#include "Player.h"
#include "ScoreManager.h"
#include <CollisionTypeIdDef.h>

void Item::Initialize(ItemType type, const Vector3& pos)
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kItem));
	Collider::SetOBBHalfSize(scale_);

	type_ = type;
	position_ = pos;
	basePosition_ = pos;

	std::string modelPath;

	object3d_ = std::make_unique<Object3D>();
	switch (type_)
	{
	case ItemType::HealSmall:
		modelPath = "cube.gltf"; break;
		object3d_->SetColor({ 1.0f,0.0f,0.0f,1.0f }); // 赤色

	case ItemType::AmmoSmall:
		modelPath = "cube.gltf"; break;
		object3d_->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f }); // 青色

	case ItemType::ScoreBonus:
		modelPath = "cube.gltf"; break;
		object3d_->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 黄色

	case ItemType::PowerUp:
		modelPath = "cube.gltf"; break;
		object3d_->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f }); // オレンジ色

	}

	object3d_->Initialize(modelPath);
	object3d_->SetTranslate(position_);
	object3d_->SetScale(scale_); // サイズを設定

	// 条件によって色を変える（変更予定）
	switch (type_)
	{
	case ItemType::HealSmall:
		object3d_->SetColor({ 1.0f,0.0f,0.0f,1.0f }); // 赤色
		break;

	case ItemType::AmmoSmall:
		object3d_->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f }); // 青色
		break;

	case ItemType::ScoreBonus:
		object3d_->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 黄色
		break;

	case ItemType::PowerUp:
		object3d_->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f }); // オレンジ色
		break;

	}
}

void Item::Update()
{
	if (collected_) return;

	// ライフタイム更新
	lifetime_ += 1.0f / 60.0f;

	// 浮遊アニメーション：サイン波でY位置を変更
	floatTimer_ += floatSpeed_ * (1.0f / 60.0f);  // フレーム前提。可変FPSなら deltaTime を使う
	float floatOffset = std::sinf(floatTimer_) * floatAmplitude_;
	position_.y = basePosition_.y + floatOffset + 1.0f;

	// Y軸を中心に回転
	rotation_.y += rotationSpeed_;

	object3d_->SetTranslate(position_);
	object3d_->SetRotate(rotation_);
	object3d_->Update();

	Collider::SetCenterPosition(position_);
}

void Item::Draw()
{
	if (!collected_ && object3d_) {
		object3d_->Draw();
	}
}

bool Item::CheckCollisionWithPlayer(const Vector3& playerPos)
{
	float pickupRadius = 2.0f;
	Vector3 diff = position_ - playerPos;
	return Vector3::Length(diff) <= pickupRadius * pickupRadius;
}

void Item::ApplyTo(Player* player)
{
	if (collected_) return;

	switch (type_)
	{
	case ItemType::HealSmall:
		player->AddHP(300); // 仮値
		break;
	case ItemType::AmmoSmall:
		player->AddAmmo(30); // 仮値
		break;
	case ItemType::ScoreBonus:
		ScoreManager::GetInstance()->AddScore(100);
		break;
	case ItemType::PowerUp:
		// 将来的な強化
		break;
	}

	collected_ = true;
}

void Item::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		ApplyTo(static_cast<Player*>(other));
	}
}
