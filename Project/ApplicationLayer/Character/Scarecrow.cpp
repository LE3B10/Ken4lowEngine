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
	model_->SetScale({ 1.0f, 2.0f, 1.0f });
	model_->SetTranslate({ 0.0f, 2.0f, 15.0f });
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Scarecrow::Update()
{
	// モデルの更新
	model_->Update();

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
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
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
