#include "Hammer.h"


/// -------------------------------------------------------------
///						　	初期化処理
/// -------------------------------------------------------------
void Hammer::Initialize()
{
	object_ = std::make_unique<Object3D>();
	object_->Initialize("Hammer/Hammer.gltf"); // ハンマーの3Dモデル
	worldTransform_.Initialize();
	worldTransform_.translation_ = { 0.0f, 4.5f, 0.0f }; // 初期位置（手元）
}


/// -------------------------------------------------------------
///						　	更新処理
/// -------------------------------------------------------------
void Hammer::Update()
{
	if (parentTransform_)
	{
		worldTransform_.translation_ = parentTransform_->translation_ + offset_; // 親の座標に同期

		// 親（プレイヤー）の回転を取得
		worldTransform_.rotate_.y = parentTransform_->rotate_.y;
	}

	// TODO: プレイヤーの腕の動きに合わせる処理
	object_->SetTranslate(worldTransform_.translation_);
	object_->SetRotate(worldTransform_.rotate_);

	// 更新処理
	object_->Update();
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Hammer::Draw()
{
	object_->Draw();
}
