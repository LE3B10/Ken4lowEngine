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

	// 浮遊ギミックの初期化
	InitializeFloatingGimmick();
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	// 移動処理
	Move();
	// 浮遊ギミックの更新
	UpdateFloatingGimmick();

	// 腕のアニメーション更新（移動中かどうかを渡す）
	bool isMoving = input_->PushKey(DIK_W) || input_->PushKey(DIK_S) || input_->PushKey(DIK_A) || input_->PushKey(DIK_D);
	UpdateArmAnimation(isMoving);
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


/// -------------------------------------------------------------
///							移動処理
/// -------------------------------------------------------------
void Player::Move()
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
///					　浮遊ギミックの初期化処理
/// -------------------------------------------------------------
void Player::InitializeFloatingGimmick()
{
	// 浮遊ギミックの初期化
	floatingParameter_ = 0.0f;
}


/// -------------------------------------------------------------
///					　浮遊ギミックの更新処理
/// -------------------------------------------------------------
void Player::UpdateFloatingGimmick()
{
	// 浮遊ギミックの更新
	floatingParameter_ += kFloatingStep;
	// 2πを超えたら0に戻す
	floatingParameter_ = std::fmodf(floatingParameter_, 2.0f * std::numbers::pi_v<float>);
	// 浮遊の振幅<m>
	const float floatingAmplitude = 0.3f;
	// 浮遊を座標に適用
	body_.transform.translate_.y = floatingAmplitude * sinf(floatingParameter_);
}


/// -------------------------------------------------------------
///					　腕のアニメーション更新処理
/// -------------------------------------------------------------
void Player::UpdateArmAnimation(bool isMoving)
{
	// 移動しているときは腕を速く振る
	float swingSpeed = isMoving ? 0.1f : 0.02f; // 移動時と待機時で速度を変える

	// アニメーションパラメータの更新
	armSwingParameter_ += swingSpeed;

	// 2πを超えたらリセット（ループさせる）
	armSwingParameter_ = std::fmod(armSwingParameter_, 2.0f * std::numbers::pi_v<float>);

	// 目標の振り角度をサイン波で計算
	float targetSwingAngle = kMaxArmSwingAngle * sinf(armSwingParameter_);

	// 各部位のアニメーションを適用
	if (parts_.size() >= 2) // 腕のデータが存在するか確認
	{
		// 左腕（現在の角度から目標角度に補間）
		float smoothedLeft = Vector3::Lerp(parts_[1].transform.rotate_.x, isMoving ? -targetSwingAngle : -targetSwingAngle * 0.5f, 0.2f);
		parts_[1].transform.rotate_.x = smoothedLeft;

		// 右腕（現在の角度から目標角度に補間）
		float smoothedRight = Vector3::Lerp(parts_[2].transform.rotate_.x, isMoving ? targetSwingAngle : -targetSwingAngle * 0.5f, 0.2f);
		parts_[2].transform.rotate_.x = smoothedRight;
	}
}
