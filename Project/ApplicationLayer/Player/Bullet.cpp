#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <ScoreManager.h>
#include <Enemy.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void Bullet::Initialize()
{
	// コライダーを初期化
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));
	Collider::SetOBBHalfSize({ 0.5f, 0.5f, 0.5f }); // OBBの半径を設定


	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetScale({ 0.5f,0.5f,0.5f });
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void Bullet::Update()
{
	// 位置更新
	position_ += velocity_;

	// 寿命を減らす
	lifeTime_ -= 1.0f / 60.0f;

	model_->SetTranslate(position_);
	model_->SetRotate({ 0.0f, 0.0f, 0.0f });
	model_->Update();

	// Collider の位置・回転を更新
	Collider::SetCenterPosition(position_);
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void Bullet::Draw()
{
	// 衝突したら描画しない
	if (!isDead_) model_->Draw();
}


/// -------------------------------------------------------------
///				　			衝突処理
/// -------------------------------------------------------------
void Bullet::OnCollision(Collider* other)
{
	// 衝突相手がエネミーかどうかを確認
	if (other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kEnemy)) return;

	// 衝突相手のユニークIDを取得
	uint32_t targetID = other->GetUniqueID();

	// すでに当たった相手かどうかを確認
	if (contactRecord_.Check(targetID)) {
		return; // すでに当たった相手なので無視
	}

	contactRecord_.Add(targetID); // 初めて当たった相手として記録

	// 衝突処理（ダメージなど）
	if (auto enemy = dynamic_cast<Enemy*>(other)) {
		enemy->TakeDamage(50.0f);
	}

	// パーティクルを表示（仮演出）

	// スコアを加算
	ScoreManager::GetInstance()->AddKill();

	isDead_ = true; // 単発弾の場合
}
