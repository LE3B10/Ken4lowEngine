#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <TextureManager.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "WeaponType.h"
#include "Matrix4x4.h"

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
	rifle->SetWeaponType(WeaponType::Rifle);
	rifle->Initialize();
	weapons_.push_back(std::move(rifle));

	// ショットガンの初期化
	auto shotgun = std::make_unique<Weapon>();
	shotgun->SetWeaponType(WeaponType::Shotgun);
	shotgun->Initialize();
	weapons_.push_back(std::move(shotgun));


	// HUDの初期化
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

	// 全武器に対して弾だけ更新（選択中の武器は除外）
	for (size_t i = 0; i < weapons_.size(); ++i)
	{
		if (i == currentWeaponIndex_) continue;
		weapons_[i]->UpdateBulletsOnly();
	}

	// 武器更新（すべての武器）
	if (Weapon* weapon = GetCurrentWeapon()) {
		weapon->Update();
	}

	// 発射入力
	bool isFireTriggered = input_->TriggerMouse(0) || input_->TriggerButton(XButtons.R_Trigger) || input_->TriggerKey(DIK_F);

	// 照準モードかどうかを確認
	isAiming_ = input_->PushMouse(1); // 右クリックでADS（Aim Down Sights）モード	
	camera_->SetFovY(isAiming_ ? 0.6f : 0.9f); // 照準時: 狭く、非照準時: 広く


	// 武器に応じて発射（Rifleなら押しっぱなし、Shotgunなら単発）
	if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Rifle)
	{
		if (input_->PushMouse(0))
		{
			FireWeapon();
		}
	}
	else if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Shotgun)
	{
		if (isFireTriggered)
		{
			FireWeapon();
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

	 // 現在の武器を描画
	for (const auto& weapon : weapons_) {
		weapon->Draw();  // 全ての武器の弾丸を描画する
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
	if (isAiming_)
	{
		isDashing_ = false;
	}
	else
	{
		bool isDashKey = input_->PushKey(DIK_LSHIFT);
		bool isDashButton = input_->PushButton(XButtons.B);
		isDashing_ = isDashKey || isDashButton;
	}

	// カメラの方向で回転
	if (camera_)
	{
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
	if (isAiming_)
	{  // ← ここでADS中かチェック
		currentSpeed *= adsSpeedFactor_;  // 例: 0.5f などで移動速度を半減
	}

	// 位置更新
	body_.worldTransform_.translate_ += moveInput * currentSpeed;

	if (camera_) {
		body_.worldTransform_.rotate_.y = camera_->GetRotate().y;
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


/// -------------------------------------------------------------
///				　			　 全弾取得
/// -------------------------------------------------------------
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


/// -------------------------------------------------------------
///				　			　 HP追加
/// -------------------------------------------------------------
void Player::AddHP(int amount)
{
	if (isDead_) return;
	hp_ = std::min(hp_ + static_cast<float>(amount), maxHP_);
}


/// -------------------------------------------------------------
///				　		弾丸発射処理位置
/// -------------------------------------------------------------
void Player::FireWeapon()
{
	Vector3 worldMuzzlePos;
	Vector3 forward;
	float range = weapons_[currentWeaponIndex_]->GetAmmoInfo().range;

	if (isAiming_) {
		// カメラ中心から照準に向けて発射（ADSモード）
		Vector3 cameraOrigin = camera_->GetTranslate();
		worldMuzzlePos = cameraOrigin;
		Vector3 targetPos = cameraOrigin + camera_->GetForward() * range;
		forward = Vector3::Normalize(targetPos - worldMuzzlePos);
	}
	else
	{
		// 通常時は右手（モデル上の銃口）から発射
		Vector3 localMuzzleOffset = { 5.0f, 20.0f, 5.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		Vector3 rotation = { 0.0f, body_.worldTransform_.rotate_.y, 0.0f };
		Vector3 translation = body_.worldTransform_.translate_;
		Matrix4x4 modelMatrix = Matrix4x4::MakeAffineMatrix(scale, rotation, translation);
		worldMuzzlePos = Vector3::Transform(localMuzzleOffset, modelMatrix);
		Vector3 cameraOrigin = camera_->GetTranslate();
		Vector3 targetPos = cameraOrigin + camera_->GetForward() * range;
		forward = Vector3::Normalize(targetPos - worldMuzzlePos);
	}

	weapons_[currentWeaponIndex_]->TryFire(worldMuzzlePos, forward);
}
