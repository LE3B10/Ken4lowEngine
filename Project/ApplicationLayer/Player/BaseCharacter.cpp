#include "BaseCharacter.h"


/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void BaseCharacter::Initialize()
{
	// 初期化
	body_.worldTransform_.Initialize();
}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void BaseCharacter::Update()
{
	// 体のワールド変換を更新
	body_.worldTransform_.Update();
	body_.object->SetTranslate(body_.worldTransform_.translate_);
	body_.object->SetRotate(body_.worldTransform_.rotate_);
	body_.object->Update();

	// 各部位のワールド変換を更新
	for (auto& part : parts_)
	{
		part.worldTransform_.worldRotate_ = body_.worldTransform_.worldRotate_; // 親の回転を適用
		part.worldTransform_.Update(); // 親の影響を受ける

		part.object->SetTranslate(part.worldTransform_.worldTranslate_);
		part.object->SetRotate(part.worldTransform_.worldRotate_); // ワールド回転を適用
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
