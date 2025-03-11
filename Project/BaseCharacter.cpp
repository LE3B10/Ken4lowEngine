#include "BaseCharacter.h"
#include <Object3DCommon.h>
#include <Input.h>
#include <Camera.h>
#include "ParticleManager.h"
#include "TextureManager.h"


/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void BaseCharacter::Initialize()
{
	input_ = Input::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// 初期化
	body_.transform.Initialize();
}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void BaseCharacter::Update()
{
	// 体のワールド変換を更新
	body_.transform.Update();
	body_.object->SetTranslate(body_.transform.translation_);
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
///					　		描画処理
/// -------------------------------------------------------------
void BaseCharacter::Draw()
{
	// 体を描画
	body_.object->Draw();
	// 各部位を描画
	for (auto& part : parts_)
	{
		part.object->Draw();
	}
}


/// -------------------------------------------------------------
///					　		衝突判定処理
/// -------------------------------------------------------------
void BaseCharacter::OnCollision(Collider* other)
{

}


/// -------------------------------------------------------------
///					　	中心座標を取得
/// -------------------------------------------------------------
Vector3 BaseCharacter::GetCenterPosition() const
{
	// ローカル座標でのオフセット
	const Vector3 offset = { 0.0f,1.0f,0.0f };
	// ワールド座標に変換
	Vector3 worldPosition = body_.transform.translation_ + offset;
	return worldPosition;
}
