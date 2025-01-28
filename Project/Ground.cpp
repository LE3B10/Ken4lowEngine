#include "Ground.h"



/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Ground::Initialize(Object3DCommon* object3DCommon)
{
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize(object3DCommon, "terrain.obj");
	object3D_->SetScale({ 10.0f,1.0f,10.0f });
	object3D_->SetTranslate({ 0.0f,0.0f,0.0f });
}



/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Ground::Update()
{
	object3D_->Update();
}



/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Ground::Draw()
{
	object3D_->Draw();
}
