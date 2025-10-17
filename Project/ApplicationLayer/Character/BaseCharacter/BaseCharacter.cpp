#include "BaseCharacter.h"
#include "Object3DCommon.h"

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BaseCharacter::Update()
{
	// 階層更新
	UpdateHierarchy();
}

/// -------------------------------------------------------------
///				　			階層更新
/// -------------------------------------------------------------
void BaseCharacter::UpdateHierarchy()
{
	// 体のワールド変換を更新
	body_.transform.Update();
	body_.object->SetTranslate(body_.transform.translate_);
	body_.object->SetRotate(body_.transform.rotate_);
	body_.object->Update();

	// 各部位のワールド変換を更新
	for (auto& part : parts_)
	{
		part.transform.worldRotate_ = body_.transform.worldRotate_; // 親の回転を適用
		part.transform.Update(); // 親の影響を受ける

		part.object->SetTranslate(part.transform.worldTranslate_);
		part.object->SetRotate(part.transform.worldRotate_); // ワールド回転を適用
		part.object->Update();
	}
}

/// -------------------------------------------------------------
///					　	中心座標を取得
/// -------------------------------------------------------------
Vector3 BaseCharacter::GetCenterPosition() const
{
	// ローカル座標でのオフセット
	const Vector3 offset = { 0.0f,1.0f,0.0f };
	// ワールド座標に変換
	Vector3 worldPosition = body_.transform.translate_ + offset;
	return worldPosition;
}
