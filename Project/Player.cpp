#include "Player.h"
#include "Object3DCommon.h"
#include "VectorMath.h"
#include "MatrixMath.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon, Camera* camera)
{
	input_ = Input::GetInstance();
	camera_ = camera;

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
	// 入力取得
	if (input_->PushKey(DIK_W)) // 前進
	{
		for (int32_t i = 0; i < parts_.size(); ++i)
		{
			parts_[i].worldTransform.translate.z += 0.1f;
		}
	}
	if (input_->PushKey(DIK_S)) // 後退
	{
		for (int32_t i = 0; i < parts_.size(); ++i)
		{
			parts_[i].worldTransform.translate.z -= 0.1f;
		}
	}
	if (input_->PushKey(DIK_A)) // 左移動
	{
		for (int32_t i = 0; i < parts_.size(); ++i)
		{
			parts_[i].worldTransform.translate.x -= 0.1f;
		}
	}
	if (input_->PushKey(DIK_D)) // 右移動
	{
		for (int32_t i = 0; i < parts_.size(); ++i)
		{
			parts_[i].worldTransform.translate.x += 0.1f;
		}
	}

	// 入力取得 (矢印キーで回転)
	if (input_->PushKey(DIK_LEFT))  // 左回転 (Y軸)
	{
		//rotation_.y -= 1.0f; // 回転角度を減少
	}
	if (input_->PushKey(DIK_RIGHT)) // 右回転 (Y軸)
	{
		//rotation_.y += 1.0f; // 回転角度を増加
	}
	if (input_->PushKey(DIK_UP))    // 上方向 (X軸回転)
	{
		//rotation_.x -= 1.0f; // 回転角度を減少
	}
	if (input_->PushKey(DIK_DOWN))  // 下方向 (X軸回転)
	{
		//rotation_.x += 1.0f; // 回転角度を増加
	}

	// 部位の移動をオブジェクトに反映
	for (auto& part : parts_)
	{
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}

	// プレイヤーの位置
	const Vector3& playerPosition = parts_[0].worldTransform.translate;

	// カメラの目標位置
	Vector3 targetCameraPosition = playerPosition + Vector3(0.0f, 20.0f, -50.0f);

	// カメラ位置を線形補間でスムーズに移動
	if (camera_)
	{
		Vector3 currentCameraPosition = camera_->GetTranslate();
		Vector3 newCameraPosition = Lerp(currentCameraPosition, targetCameraPosition, 0.05f);
		camera_->SetTranslate(newCameraPosition);
		camera_->SetTargetPosition(playerPosition);
		camera_->Update();
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
