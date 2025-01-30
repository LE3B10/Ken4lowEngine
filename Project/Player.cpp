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
		// 胴体（親なし）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}, nullptr, "body.gltf", -1 },

		// 左腕（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-0.5f, 4.5f, 0.0f}}, nullptr, "L_Arm.gltf", 0 },

		// 右腕（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 4.5f, 0.0f}}, nullptr, "R_Arm.gltf", 0 },

		// 頭（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}}, nullptr, "Head.gltf", 0 }
	};

	for (size_t i = 0; i < parts_.size(); ++i)
	{
		auto& part = parts_[i];
		part.object3D = std::make_unique<Object3D>();
		part.object3D->Initialize(object3DCommon, part.modelFile);
		part.object3D->SetTranslate(part.worldTransform.translate);

		// 子オブジェクトのローカル座標を記録
		if (part.parentIndex != -1)
		{
			part.localOffset = part.worldTransform.translate - parts_[part.parentIndex].worldTransform.translate;
		}
	}
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	// 移動量を初期化
	Vector3 movement = { 0.0f, 0.0f, 0.0f };

	// 入力取得 (移動)
	if (input_->PushKey(DIK_W)) // 前進
	{
		movement.z += 0.3f;
	}
	if (input_->PushKey(DIK_S)) // 後退
	{
		movement.z -= 0.3f;
	}
	if (input_->PushKey(DIK_A)) // 左移動
	{
		movement.x -= 0.3f;
	}
	if (input_->PushKey(DIK_D)) // 右移動
	{
		movement.x += 0.3f;
	}

	// 胴体 (親) の移動を適用
	parts_[0].worldTransform.translate += movement;

	// 親の位置を考慮して各部位の最終的な位置を計算
	for (size_t i = 1; i < parts_.size(); ++i)
	{
		int parentIndex = parts_[i].parentIndex;
		if (parentIndex != -1)
		{
			// localOffset は初期化時に保存した相対座標
			parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + parts_[i].localOffset;
		}
	}

	// 各部位のオブジェクトの位置を更新
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
