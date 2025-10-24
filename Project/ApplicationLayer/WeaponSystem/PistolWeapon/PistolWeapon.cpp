#include "PistolWeapon.h"
#include "CollisionTypeIdDef.h"
#include <Input.h>

#include <imgui.h>

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void PistolWeapon::Initialize()
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kWeapon));

	// モデル読み込み：（武器のモデルに後で変える）
	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetScale({ 0.1f, 0.1f, 0.8f }); // ピストルっぽく
	model_->SetColor({ 0.2f, 0.2f, 0.2f, 1.0f });
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void PistolWeapon::Update(float deltaTime)
{
	(void)deltaTime; // 未使用

	// 親のTransformが設定されていれば追従
	if (parentTransform_)
	{
		// 90度（π/2ラジアン）
		const float kHalfPi = std::numbers::pi_v<float> / 2.0f;

		// 親のワールド変換と指定されたオフセットや回転を基に、オブジェクトのワールド変換を更新する。
		transform_.Update(parentTransform_, offset_, kHalfPi, { kHalfPi,0.0f,0.0f });
	}

	// モデルのワールド変換を更新
	model_->SetTranslate(transform_.translate_);
	model_->SetRotate(transform_.rotate_);
	model_->Update();
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void PistolWeapon::Draw()
{
	model_->Draw();
}

/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void PistolWeapon::DrawImGui()
{
	// 武器の座標表示
	ImGui::Text("Weapon Position: (%.2f, %.2f, %.2f)", transform_.translate_.x, transform_.translate_.y, transform_.translate_.z);

	// オフセット調整
	ImGui::DragFloat3("Offset", &offset_.x, 0.01f);
}

/// -------------------------------------------------------------
///				　			　 リロード
/// -------------------------------------------------------------
void PistolWeapon::Reload()
{
}

/// -------------------------------------------------------------
///				　			　 衝突時処理
/// -------------------------------------------------------------
void BaseWeapon::OnCollision(Collider* other)
{
	(void)other; // 他のオブジェクトと衝突したときの処理をここに実装
}

/// -------------------------------------------------------------
///				　			　 衝突時処理
/// -------------------------------------------------------------
void PistolWeapon::OnCollision(Collider* other)
{
	// 他のオブジェクトと衝突したときの処理をここに実装
	(void)other;
}

/// -------------------------------------------------------------
///				　			　 中心座標取得
/// -------------------------------------------------------------
Vector3 PistolWeapon::GetCenterPosition() const
{
	// 行列を最新に更新
	const_cast<WorldTransformEx&>(transform_).Update();

	// オフセット（ローカル座標系）
	const Vector3 offet = { 0.0f,0.0f,0.0f };

	// ワールド行列を使って座標変換
	position_ = Matrix4x4::Transform(offet, transform_.worldMatrix_);

	// 座標を返す
	return position_;
}
