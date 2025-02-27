#include "Player.h"
#include <Object3DCommon.h>
#include <ModelManager.h>
#include <Input.h>
#include <Camera.h>


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();

	// 体（親）の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Player/body.gltf");
	body_.transform.Initialize();
	body_.transform.translate_ = { 0, 0, 0 };

	// 子オブジェクト（頭、腕）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"Player/Head.gltf", {0, 5.0f, 0}},     // 頭
		{"Player/L_arm.gltf", {0.0f, 4.5f, 0}}, // 左腕
		{"Player/R_arm.gltf", {0.0f, 4.5f, 0}}  // 右腕
	};

	for (const auto& [modelPath, position] : partData)
	{
		BodyPart part;
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.transform.Initialize();
		part.transform.translate_ = position;
		part.object->SetTranslate(part.transform.translate_);
		part.transform.parent_ = &body_.transform; // 親を設定
		parts_.push_back(std::move(part));
	}
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	if (camera_)
	{
		Vector3 move = { 0, 0, 0 };

		// 入力処理
		if (input_->PushKey(DIK_W)) { move.z += moveSpeed_; } // 前進
		if (input_->PushKey(DIK_S)) { move.z -= moveSpeed_; } // 後退
		if (input_->PushKey(DIK_A)) { move.x -= moveSpeed_; } // 左移動
		if (input_->PushKey(DIK_D)) { move.x += moveSpeed_; } // 右移動

		if (move.x != 0 || move.z != 0) // 移動している時だけ向きを変える
		{
			float yaw = camera_->GetRotate().y;

			// Yawによる移動ベクトルの回転
			Vector3 rotatedMove;
			rotatedMove.x = move.x * cosf(yaw) - move.z * sinf(yaw);
			rotatedMove.z = move.x * sinf(yaw) + move.z * cosf(yaw);

			// 向きを滑らかに補間 (度数法に変換)
			float targetAngle = atan2(-rotatedMove.x, rotatedMove.z) * (180.0f / std::numbers::pi_v<float>);
			float smoothAngle = Vector3::AngleLerp(body_.transform.rotate_.y * (180.0f / std::numbers::pi_v<float>), targetAngle, 0.2f);
			body_.transform.rotate_.y = smoothAngle * (std::numbers::pi_v<float> / 180.0f);

			// 正規化して移動
			rotatedMove = Vector3::Normalize(rotatedMove);
			body_.transform.translate_ += rotatedMove * moveSpeed_;
		}

		// 体のワールド変換を更新
		body_.transform.Update();
		body_.object->SetTranslate(body_.transform.translate_);
		body_.object->SetRotate(body_.transform.rotate_);
		body_.object->Update();

		// 各部位のワールド変換を更新
		for (auto& part : parts_)
		{
			part.transform.worldRotate_ = body_.transform.worldRotate_; // 親の回転を適用
			part.transform.Update(); // 親の影響を受ける

			part.object->SetTranslate(part.transform.worldTranslate_);
			part.object->SetRotate(part.transform.worldRotate_); // ワールド回転を適用
			part.object->Update();
		}
	}
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// 体を描画
	body_.object->Draw();

	// 各部位を描画
	for (auto& part : parts_)
	{
		part.object->Draw();
	}
}
