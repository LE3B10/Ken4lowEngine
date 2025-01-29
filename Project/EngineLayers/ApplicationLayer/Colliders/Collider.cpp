#include "Collider.h"
#include "ParameterManager.h"

/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void Collider::Initialize(Object3DCommon* object3DCommon)
{
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize(object3DCommon, "Collider.gltf");
	worldTransform_ = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f } };
}


/// -------------------------------------------------------------
///						　　更新処理
/// -------------------------------------------------------------
void Collider::Update()
{
	object3D_->SetScale(worldTransform_.scale);
	object3D_->SetRotate(worldTransform_.rotate);
	object3D_->SetTranslate(worldTransform_.translate);
	object3D_->Update();
}


/// -------------------------------------------------------------
///						　　描画処理
/// -------------------------------------------------------------
void Collider::Draw()
{
	object3D_->Draw();
}
