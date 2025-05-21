#include "Player.h"
#include <Object3DCommon.h>
#include <TextureManager.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// 基底クラスの初期化
	BaseCharacter::Initialize();

	// プレイヤーのコライダーを初期化
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	Collider::SetOBBHalfSize({ 2.5f, 6.0f, 2.5f }); // OBBの半径を設定

	input_ = Input::GetInstance();

	// 体の部位の初期化
	InitializeParts();

	// プレイヤーコントローラーの生成と初期化
	controller_ = std::make_unique<PlayerController>();
	controller_->Initialize();

	// 武器
	weapon_.Initialize();

	numberSpriteDrawer_ = std::make_unique<NumberSpriteDrawer>();
	numberSpriteDrawer_->Initialize("Resources/number.png", 50.0f, 50.0f);

	hp_ = 100.0f;
	isDead_ = false;
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	if (isDead_) return; // 死亡後は行動不可

	// プレイヤーの移動処理
	Move();

	// --- 発射入力（マウス左クリックまたはゲームパッドRT） ---
	// 発射入力
	bool isFireTriggered = input_->TriggerMouse(0) || input_->TriggerButton(XButtons.R_Trigger) || input_->TriggerKey(DIK_F);

	if (isFireTriggered)
	{
		Vector3 startPos = body_.worldTransform_.translate_ + Vector3{ 0.0f, 20.0f, 0.0f };
		Vector3 fireDir = camera_->GetForward();
		weapon_.TryFire(startPos, fireDir); // ★ ここで撃つ
	}

	weapon_.Update(); // ★ 弾の更新やリロード処理

	// コライダーの位置と回転をプレイヤーに追従させる
	Collider::SetCenterPosition(body_.worldTransform_.translate_ + Vector3(0.0f, 8.2f, 0.0f));

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
	weapon_.Draw(); // ★ 弾丸を描画
}


/// -------------------------------------------------------------
///				　			　 HUD描画処理
/// -------------------------------------------------------------
void Player::DrawHUD()
{
	numberSpriteDrawer_->Reset(); // 数字スプライト描画クラスのリセット
	numberSpriteDrawer_->DrawNumber(weapon_.GetAmmoInClip(), { 1000.0f, 620.0f });  // HUDに数字を描画（弾丸数）
	numberSpriteDrawer_->DrawNumber(weapon_.GetAmmoReserve(), { 1120.0f, 620.0f }); // HUDに数字を描画（総弾丸数）
}


/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
	weapon_.DrawImGui(); // ★ 武器のImGui描画
}

void Player::TakeDamage(float damage)
{
	if (isDead_) return;
	hp_ -= damage;
	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;
		isDead_ = true;
		Log("Player is dead");
	}
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

void Player::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kDefault))
	{
		return;
	}
}
