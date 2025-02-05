#include "BaseCharacter.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BaseCharacter::Initialize(Object3DCommon* object3DCommon, Camera* camera)
{
	input_ = Input::GetInstance();
	camera_ = camera;
}


/// -------------------------------------------------------------
///							後進処理
/// -------------------------------------------------------------
void BaseCharacter::Update()
{
	// 共通の更新処理

}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void BaseCharacter::Draw()
{
	for (auto& part : parts_)
	{
		part.object3D->Draw();
	}
}

Vector3 BaseCharacter::GetCenterPosition() const
{
	return Vector3();
}
