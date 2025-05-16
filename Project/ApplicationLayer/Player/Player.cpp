#include "Player.h"
#include <Object3DCommon.h>
#include <TextureManager.h>


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// 基底クラスの初期化
	BaseCharacter::Initialize();

	// 体の部位の初期化
	InitializeParts();

	// プレイヤーコントローラーの生成と初期化
	controller_ = std::make_unique<PlayerController>();
	controller_->Initialize();
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	// プレイヤーの移動処理
	Move();

	// 移動が完了した後に親子更新・描画行列を更新する
	BaseCharacter::Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// 基底クラスの描画
	//BaseCharacter::Draw();
}


/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{

}


/// -------------------------------------------------------------
///				　 プレイヤー専用パーツの初期化
/// -------------------------------------------------------------
void Player::InitializeParts()
{
	// 親オブジェクトの生成と初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("body.gltf");

	// 子オブジェクト（頭、腕）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"Head.gltf", {0, 5.0f, 0}},     // 頭
		{"L_arm.gltf", {0.0f, 4.5f, 0}}, // 左腕（赤）
		{"R_arm.gltf", {0.0f, 4.5f, 0}}  // 右腕（青）
	};

	// 各部位の初期化
	for (const auto& [modelPath, position] : partData)
	{
		BodyPart part;
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.worldTransform_.Initialize();
		part.worldTransform_.translate_ = position;
		part.object->SetTranslate(part.worldTransform_.translate_);
		part.worldTransform_.parent_ = &body_.worldTransform_;
		parts_.push_back(std::move(part));
	}
}


/// -------------------------------------------------------------
///				　			　 移動処理
/// -------------------------------------------------------------
void Player::Move()
{
	// プレイヤーコントローラーの更新
	controller_->Update();

	// プレイヤーの移動入力を取得
	Vector3 moveInput = controller_->GetMoveInput();

	// カメラの向きを考慮して移動方向を回転（YawだけでOK）
	if (camera_)
	{
		float yaw = camera_->GetRotate().y;

		float sinY = sinf(yaw);
		float cosY = cosf(yaw);

		// 入力をカメラの回転方向に変換
		Vector3 rotatedInput = {
			moveInput.x * cosY - moveInput.z * sinY,
			0.0f,
			moveInput.x * sinY + moveInput.z * cosY
		};

		moveInput = rotatedInput;
	}

	// プレイヤーの移動処理
	const float speed = 0.2f;
	body_.worldTransform_.translate_ += moveInput * speed;

	// 入力方向に向ける処理
	if (Vector3::Length(moveInput) > 0.0f)
	{
		float targetAngle = std::atan2(-moveInput.x, moveInput.z);
		body_.worldTransform_.rotate_.y = Vector3::LerpAngle(body_.worldTransform_.rotate_.y, targetAngle, 0.2f);
	}

	// --- ジャンプ処理 ---
	if (isGrounded_ && controller_->IsJumpTriggered())
	{
		velocity_.y = jumpPower_;   // 上方向へ加速
		isGrounded_ = false;
	}

	// --- 重力によるY軸移動 ---
	velocity_.y += gravity_ * deltaTime; // ※ deltaTime はフレーム間時間（例: 1/60.0f）
	body_.worldTransform_.translate_.y += velocity_.y;

	// --- 簡易的な地面との接地判定（Y = 0 が地面） ---
	if (body_.worldTransform_.translate_.y <= 0.0f)
	{
		body_.worldTransform_.translate_.y = 0.0f;
		velocity_.y = 0.0f;
		isGrounded_ = true;
	}
}
