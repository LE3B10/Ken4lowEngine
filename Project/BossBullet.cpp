#include "BossBullet.h"
#include <CollisionTypeIdDef.h>
#include "Player.h"

void BossBullet::Initialize()
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet)); // 新たなIDを定義
	Collider::SetOBBHalfSize({});
	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf"); // 敵弾用の異なるモデルにするなら
	model_->SetScale({ 0.5f, 0.5f, 0.5f });

	// 初期位置を前回位置として記録（重要）
	previousPosition_ = position_;
}

void BossBullet::Update()
{
	previousPosition_ = position_;
	position_ += velocity_ * 8.0f;

	// Segment計算（マージン追加）
	Vector3 direction = position_ - previousPosition_;
	Vector3 normalized = Vector3::Normalize(direction);
	float margin = 0.2f;
	segment_.origin = previousPosition_;
	segment_.diff = direction + normalized * margin;

	// モデル更新
	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	// コライダー情報を更新
	SetCenterPosition(position_);
	SetSegment(segment_);

	lifeTime_ -= 1.0f / 60.0f;
}

void BossBullet::Draw()
{
	if (!isDead_) model_->Draw();
}

void BossBullet::OnCollision(Collider* other)
{
	// 衝突相手がプレイヤーかどうかを確認
	if (other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) return;

	// 衝突相手のユニークIDを取得
	uint32_t targetID = other->GetUniqueID();

	// すでに当たった相手かどうかを確認
	if (contactRecord_.Check(targetID)) return; // 多段ヒット防止

	// 初めて当たった相手として記録
	contactRecord_.Add(targetID);

	// プレイヤーにダメージを与える
	Player* player = dynamic_cast<Player*>(other);

	// プレイヤーが存在する場合、ダメージを与える
	if (player) {
		player->TakeDamage(200.0f); // 任意のダメージ
	}

	// パーティクルを表示（仮演出）

	// 死亡フラグを立てる
	isDead_ = true;
}
