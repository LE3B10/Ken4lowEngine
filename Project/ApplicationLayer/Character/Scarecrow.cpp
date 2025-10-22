#include "Scarecrow.h"
#include "CollisionTypeIdDef.h"

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Scarecrow::Initialize()
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));

	// モデルの初期化
	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetScale(scale_);		 // かかしの大きさに調整
	model_->SetTranslate(position_); // 地面から少し上に配置
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Scarecrow::Update()
{
	// モデルの更新
	model_->Update();

	// コライダーの更新
	Collider::SetOBBHalfSize(model_->GetScale());
	Collider::SetCenterPosition(model_->GetTranslate());
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Scarecrow::Draw()
{
	// モデルの描画
	model_->Draw();
}

/// -------------------------------------------------------------
///				　		　衝突時処理
/// -------------------------------------------------------------
void Scarecrow::OnCollision(Collider* other)
{
	// 相手がプレイヤーの場合
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		// 衝突判定時の処理
		OutputDebugStringA("Scarecrow collided with Player!\n");
	}
}

/// -------------------------------------------------------------
///				　		中心座標取得
/// -------------------------------------------------------------
Vector3 Scarecrow::GetCenterPosition() const
{
	return Vector3();
}
