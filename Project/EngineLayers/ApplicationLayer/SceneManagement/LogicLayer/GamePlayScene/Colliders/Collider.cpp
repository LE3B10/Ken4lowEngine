#include "Collider.h"
#include "Object3DCommon.h"

/// -------------------------------------------------------------
///				　　　初期化処理・可視化処理用
/// -------------------------------------------------------------
void Collider::Initialize(Object3DCommon* object3DCommon)
{
	std::unique_ptr<Object3DCommon> object3DCommon = Object3DCommon::GetInstance();

	// コライダーオブジェクトの生成と初期化
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize();
}


/// -------------------------------------------------------------
///					　更新処理・可視化処理用
/// -------------------------------------------------------------
void Collider::Update()
{
	//// コライダーオブジェクトの更新処理
	object3D_->Update();
}


/// -------------------------------------------------------------
///				　	　描画処理・可視化処理用
/// -------------------------------------------------------------
void Collider::Draw()
{
	// コライダーオブジェクトの描画処理
	object3D_->Draw();
}
