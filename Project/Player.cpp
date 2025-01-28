#include "Player.h"
#include "Object3DCommon.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon)
{
	parts_ = {
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}, nullptr, "body.gltf" },
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-0.5f, 4.5f, 0.0f}}, nullptr, "L_Arm.gltf" },
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 4.5f, 0.0f}}, nullptr, "R_Arm.gltf" },
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}}, nullptr, "Head.gltf" }
	};

	for (auto& part : parts_)
	{
		part.object3D = std::make_unique<Object3D>();
		part.object3D->Initialize(object3DCommon, part.modelFile);
		part.object3D->SetTranslate(part.worldTransform.translate);
	}
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	for (auto& part : parts_)
	{
		part.object3D->Update();
	}
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	for (auto& part : parts_)
	{
		part.object3D->Draw();
	}
}
