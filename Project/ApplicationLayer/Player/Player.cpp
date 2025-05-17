#include "Player.h"
#include <Object3DCommon.h>
#include <TextureManager.h>
#include <Input.h>


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// 基底クラスの初期化
	BaseCharacter::Initialize();
	input_ = Input::GetInstance();

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

	// --- 発射入力（マウス左クリックまたはゲームパッドRT） ---
	bool isFireTriggered = input_->TriggerMouse(0) || input_->TriggerButton(XButtons.R_Trigger) || input_->TriggerKey(DIK_F);

	if (isFireTriggered)
	{
		// 弾の再生成と初期化
		auto bullet = std::make_unique<Bullet>();
		bullet->Initialize();

		// 発射位置（プレイヤーの位置 + 頭部オフセット）
		Vector3 startPos = body_.worldTransform_.translate_ + Vector3{ 0.0f, 20.0f, 0.0f };

		// カメラから方向を取得（前方ベクトル）
		Vector3 fireDir = camera_->GetForward(); // ※正規化されている前提
		const float speed = 40.0f;

		bullet->SetPosition(startPos);
		bullet->SetVelocity(fireDir * speed);

		bullets_.emplace_back(std::move(bullet)); // 新しい弾を追加
	}

	// 弾の更新 ＆ 寿命が切れたら削除
	for (auto it = bullets_.begin(); it != bullets_.end();)
	{
		(*it)->Update();

		// 寿命切れで削除
		if ((*it)->IsDead()) {
			it = bullets_.erase(it);
		}
		else {
			++it;
		}
	}

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

	// 弾丸の描画
	for (auto& bullet : bullets_) {
		bullet->Draw();
	}
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
