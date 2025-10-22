#include "PistolWeapon.h"
#include "CollisionTypeIdDef.h"
#include <Input.h>

#include <imgui.h>

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void PistolWeapon::Initialize(const WeaponConfig& config)
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kWeapon));

	weaponConfig_ = config; // 武器設定を保存

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
	(void)deltaTime;

	// 親Transformが設定されていれば追従
	if (parentTransform_)
	{
		// 親の回転を引き継ぐ
		Matrix4x4 Rx = Matrix4x4::MakeRotateX(parentTransform_->rotate_.x);
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(parentTransform_->rotate_.y);
		Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);

		// 右腕で -90°入れている分を +90°で打ち消してから親回転へ
		Matrix4x4 RxFix = Matrix4x4::MakeRotateX(+90.0f * std::numbers::pi_v<float> / 180.0f);
		Vector3   ofsLocalFixed = Matrix4x4::Transform(offset_, RxFix);

		Vector3 ofsW = Matrix4x4::Transform(ofsLocalFixed, R);
		transform_.translate_ = parentTransform_->translate_ + ofsW;

		// 親の回転をそのまま継承（必要なら+ローカル微調整）
		transform_.rotate_ = parentTransform_->rotate_;
		const float kHalfPi = std::numbers::pi_v<float> / 2.0f;
		transform_.rotate_.x += kHalfPi; // 右腕補正分を追加
	}

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

	const Vector3 offet = { 0.0f,0.0f,0.0f };

	position_ = Matrix4x4::Transform(offet, transform_.worldMatrix_);

	return position_;
}
