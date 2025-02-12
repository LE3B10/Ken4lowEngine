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


/// -------------------------------------------------------------
///						　中心座標の処理
/// -------------------------------------------------------------
Vector3 BaseCharacter::GetCenterPosition() const
{
	// ワールド座標を入れる変数
	Vector3 worldPosition{
		parts_[0].worldTransform.translate.x,
		parts_[0].worldTransform.translate.y,
		parts_[0].worldTransform.translate.z,
	};

	return worldPosition;
}
