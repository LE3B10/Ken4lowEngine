#include "Player.h"
#include <Object3DCommon.h>
#include <TextureManager.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "WeaponType.h"
#include <imgui.h>


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

	// ライフルの初期化
	auto rifle = std::make_unique<Weapon>();
	rifle->Initialize();
	rifle->SetWeaponType(WeaponType::Rifle);
	weapons_.push_back(std::move(rifle));

	auto shotgun = std::make_unique<Weapon>();
	shotgun->Initialize();
	shotgun->SetWeaponType(WeaponType::Shotgun);
	// カスタム設定があればここで弾数や発射速度など変更
	weapons_.push_back(std::move(shotgun));

	numberSpriteDrawer_ = std::make_unique<NumberSpriteDrawer>();
	numberSpriteDrawer_->Initialize("Resources/number.png", 50.0f, 50.0f);
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

	// 武器切り替え（1, 2キー）
	if (input_->TriggerKey(DIK_1)) currentWeaponIndex_ = 0;
	if (input_->TriggerKey(DIK_2)) currentWeaponIndex_ = 1;

	// 武器更新（すべての武器）
	for (auto& weapon : weapons_) {
		weapon->Update();
	}

	// 発射入力
	bool isFireTriggered = input_->TriggerMouse(0) || input_->TriggerButton(XButtons.R_Trigger) || input_->TriggerKey(DIK_F);

	Vector3 firePos = body_.worldTransform_.translate_ + Vector3{ 0.0f, 20.0f, 0.0f };;   // 弾発射の開始位置（関数名は仮）
	Vector3 direction = camera_->GetForward();             // プレイヤーの向き

	if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Rifle)
	{
		if (input_->PushMouse(0))
		{
			GetCurrentWeapon()->TryFire(firePos, direction);
		}
	}
	else if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Shotgun)
	{
		if (input_->TriggerMouse(0))
		{
			GetCurrentWeapon()->TryFire(firePos, direction);
		}
	}

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
	for (auto& weapon : weapons_) {
		weapon->Draw();
	}
}


/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
	//weapon_.DrawImGui(); // ★ 武器のImGui描画

	std::string weaponName = "Unknown";
	switch (GetCurrentWeapon()->GetWeaponType())
	{
	case WeaponType::Rifle:
		weaponName = "Rifle";
		break;

	case WeaponType::Shotgun:
		weaponName = "Shotgun";
		break;
	}

	ImGui::Text("Current Weapon: %s", weaponName.c_str());
}


/// -------------------------------------------------------------
///				　			　 ダメージ処理
/// -------------------------------------------------------------
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
	controller_->Update();
	Vector3 moveInput = controller_->GetMoveInput();

	// --- ダッシュ入力確認 ---
	bool isDashKey = input_->PushKey(DIK_LSHIFT);
	bool isDashButton = input_->PushButton(XButtons.B);
	isDashing_ = isDashKey || isDashButton;

	// カメラの方向で回転
	if (camera_) {
		float yaw = camera_->GetRotate().y;
		float sinY = sinf(yaw);
		float cosY = cosf(yaw);
		moveInput = {
			moveInput.x * cosY - moveInput.z * sinY,
			0.0f,
			moveInput.x * sinY + moveInput.z * cosY
		};
	}

	// --- ダッシュ適用移動速度 ---
	float currentSpeed = baseSpeed_;
	if (isDashing_) {
		currentSpeed *= dashMultiplier_;
	}

	// 位置更新
	body_.worldTransform_.translate_ += moveInput * currentSpeed;

	// 入力方向を向く
	if (Vector3::Length(moveInput) > 0.0f) {
		float targetAngle = std::atan2(-moveInput.x, moveInput.z);
		body_.worldTransform_.rotate_.y = Vector3::LerpAngle(body_.worldTransform_.rotate_.y, targetAngle, 0.2f);
	}

	// --- ジャンプ・重力・接地（現状維持） ---
	if (isGrounded_ && controller_->IsJumpTriggered()) {
		velocity_.y = jumpPower_;
		isGrounded_ = false;
	}

	velocity_.y += gravity_ * deltaTime;
	body_.worldTransform_.translate_.y += velocity_.y;

	if (body_.worldTransform_.translate_.y <= 0.0f) {
		body_.worldTransform_.translate_.y = 0.0f;
		velocity_.y = 0.0f;
		isGrounded_ = true;
	}
}


/// -------------------------------------------------------------
///				　			　 衝突処理
/// -------------------------------------------------------------
void Player::OnCollision(Collider* other)
{

}


std::vector<const Bullet*> Player::GetAllBullets() const
{
	std::vector<const Bullet*> allBullets;
	for (const auto& weapon : weapons_)
	{
		for (const auto& bullet : weapon->GetBullets())
		{
			allBullets.push_back(bullet.get());
		}
	}
	return allBullets;
}