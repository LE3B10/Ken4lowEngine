#include "BossBullet.h"
#include <CollisionTypeIdDef.h>
#include "Player.h"

void BossBullet::Initialize()
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet)); // 新たなIDを定義

	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf"); // 敵弾用の異なるモデルにするなら
	model_->SetScale({ 0.1f, 0.1f, 0.1f });

	// 初期位置を前回位置として記録（重要）
	previousPosition_ = position_;
}

void BossBullet::Update()
{
	// 位置更新前に記録
	previousPosition_ = position_;
	position_ += velocity_;

	// 飛距離計算
	distanceTraveled_ += Vector3::Length(velocity_);

	// セグメント更新（マージンを追加して通過を防ぐ）
	Vector3 direction = position_ - previousPosition_;
	Vector3 normalized = Vector3::Normalize(direction);
	float margin = 0.2f;
	segment_.origin = previousPosition_;
	segment_.diff = direction + normalized * margin;

	// 描画・当たり判定更新
	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	SetCenterPosition(position_);
	SetSegment(segment_);
}

void BossBullet::Draw()
{
	// 衝突したら描画しない
	if (!isDead_ && distanceTraveled_ < maxDistance_) {
		//model_->Draw();
	}
}

void BossBullet::OnCollision(Collider* other)
{
	// 衝突相手が nullptrの場合は処理をスキップ
	if (other == nullptr) return;

	// 衝突相手が「敵系」以外なら無視 
	if (other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) return;

	// 衝突相手のユニークIDを取得
	uint32_t targetID = other->GetUniqueID();

	// すでに当たった相手かどうかを確認
	if (contactRecord_.Check(targetID)) return; // すでに当たった相手なので無視

	contactRecord_.Add(targetID); // 初めて当たった相手として記録

	if (auto player = other->GetOwner<Player>())        // ★ 追加
	{
		player->TakeDamage(GetDamage() * 0.125f);
	}

	isDead_ = true;
}
