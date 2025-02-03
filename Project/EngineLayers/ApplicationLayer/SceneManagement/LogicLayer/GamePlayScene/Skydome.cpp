#include "Skydome.h"
#include "Object3DCommon.h"

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void Skydome::Initialize(Object3DCommon* object3DCommon)
{
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize(object3DCommon, "skydome.gltf");
	object3D_->SetScale({ 150.0f,150.0f,150.0f });
	object3D_->SetTranslate({ 0.0f,0.0f,0.0f });
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void Skydome::Update()
{
	object3D_->Update();
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void Skydome::Draw()
{
	object3D_->Draw();
}
