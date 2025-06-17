#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <TextureManager.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "WeaponType.h"
#include "Matrix4x4.h"
#include <AudioManager.h>
#include <AnimationPipelineBuilder.h>

#include <imgui.h>
#include <PostEffectManager.h>


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// プレイヤーのコライダーを初期化
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	Collider::SetOBBHalfSize({ 1.0f, 1.0f, 1.0f }); // OBBの半径を設定

	input_ = Input::GetInstance();

	// アニメーションモデルの初期化
	animationModel_ = std::make_unique<AnimationModel>();
	animationModel_->Initialize("human.gltf");
	animationModel_->SetSkinningEnabled(true);

	// 武器の初期化
	InitializeWeapons();

	// プレイヤーコントローラーの生成と初期化
	controller_ = std::make_unique<PlayerController>();
	controller_->Initialize();

	// HUDの初期化
	numberSpriteDrawer_ = std::make_unique<NumberSpriteDrawer>();
	numberSpriteDrawer_->Initialize("Resources/number.png", 50.0f, 50.0f);

	weapons_[currentWeaponIndex_]->SetPlayer(this);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{

	if (input_->TriggerKey(DIK_H)) {
		TakeDamage(10);
	}

	if (isDead_) return; // 死亡後は行動不可

	weapons_[currentWeaponIndex_]->SetFpsCamera(fpsCamera_);

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

	// 照準モードかどうかを確認
	if (!weapons_[currentWeaponIndex_]->IsReloading()) // リロード中は発射不可
	{
		isAiming_ = input_->PushMouse(1); // 右クリックでADS（Aim Down Sights）モード
	}
	else
	{
		isAiming_ = false; // リロード中はADS不可
	}

	camera_->SetFovY(isAiming_ ? 0.6f : 0.9f); // 照準時: 狭く、非照準時: 広く

	// 武器に応じて発射（Rifleなら押しっぱなし、Shotgunなら単発）
	if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Rifle)
	{
		if (controller_->IsPushShooting())
		{
			FireWeapon();
		}
	}
	else if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Shotgun)
	{
		if (controller_->IsTriggerShooting())
		{
			FireWeapon();
		}
	}

	if (animationModel_)
	{
		animationModel_->Update();
	}

	// スタミナシステムの更新
	UpdateStamina();

	if (isHitEffectActive_)
	{
		hitEffectTimer_ -= deltaTime;
		if (hitEffectTimer_ <= 0.0f)
		{
			isHitEffectActive_ = false;
			PostEffectManager::GetInstance()->DisableEffect("RadialBlurEffect");
		}
	}

	// コライダーの位置と回転をプレイヤーに追従させる
	Collider::SetCenterPosition(animationModel_->GetTranslate());
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// 現在の武器を描画
	for (const auto& weapon : weapons_) {
		weapon->Draw();  // 全ての武器の弾丸を描画する
	}

	if (animationModel_)
	{
		AnimationPipelineBuilder::GetInstance()->SetRenderSetting();
		//animationModel_->Draw();
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

	ImGui::Separator();
	ImGui::Text("== Stamina Info ==");
	ImGui::Text("Stamina: %.1f / %.1f", stamina_, maxStamina_);
	ImGui::ProgressBar(stamina_ / maxStamina_, ImVec2(200, 20));
	ImGui::Text("Recover Delay: %.2f / %.2f", staminaRecoverTimer_, staminaRecoverDelay_);
	ImGui::Text("Recover Blocked: %s", isStaminaRecoverBlocked_ ? "Yes" : "No");
}


/// -------------------------------------------------------------
///				　			　 ダメージ処理
/// -------------------------------------------------------------
void Player::TakeDamage(float damage)
{
	if (isDead_) return; // 既に死亡している場合は何もしない

	hp_ -= damage;

	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;
		isDead_ = true;
		Log("Player is dead");
	}

	// 被弾ポストエフェクトを起動
	hitEffectTimer_ = 0.3f; // 表示する秒数
	isHitEffectActive_ = true;

	// エフェクトON
	PostEffectManager::GetInstance()->EnableEffect("RadialBlurEffect");
}


/// -------------------------------------------------------------
///				　			　 武器の初期化
/// -------------------------------------------------------------
void Player::InitializeWeapons()
{
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
}


/// -------------------------------------------------------------
///				　			　 移動処理
/// -------------------------------------------------------------
void Player::Move()
{
	controller_->Update();
	Vector3 moveInput = controller_->GetMoveInput();
	isCrouching_ = controller_->IsCrouchPressed(); // しゃがみ状態を取得

	// --- ダッシュ入力確認 ---
	bool isDashKey = input_->PushKey(DIK_LSHIFT);
	bool isDashButton = input_->PushButton(XButtons.B);

	// --- ダッシュ入力確認 ---
	if (isAiming_ || weapons_[currentWeaponIndex_]->IsReloading())
	{
		isDashing_ = false; // リロード中はダッシュ不可
		moveInput *= 0.5f; // 例: リロード中は移動速度を半減
	}
	else if ((isDashKey || isDashButton) && (moveInput.x != 0.0f || moveInput.z != 0.0f) && stamina_ >= staminaDashCost_ * deltaTime)
	{
		isDashing_ = true;
		stamina_ -= staminaDashCost_ * deltaTime;
		isStaminaRecoverBlocked_ = true;
		staminaRecoverTimer_ = 0.0f;
	}
	else
	{
		isDashing_ = false;
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
	if (isDashing_) currentSpeed *= dashMultiplier_;
	if (isAiming_)	currentSpeed *= adsSpeedFactor_;  // 例: 0.5f などで移動速度を半減
	if (isCrouching_) currentSpeed *= crouchingSpeed_; // しゃがみ時は移動速度を半減

	// 位置更新
	Vector3 modelPosition = animationModel_->GetTranslate();
	modelPosition += moveInput * currentSpeed;
	animationModel_->SetTranslate(modelPosition);

	if (camera_)
	{
		float yaw = animationModel_->GetRotate().y;
		animationModel_->SetRotate({ 0.0f,yaw,0.0f });
	}

	// --- ジャンプ・重力・接地（修正版） ---
	Vector3 position = animationModel_->GetTranslate();

	// ジャンプ開始
	if (isGrounded_ && controller_->IsJumpTriggered())
	{
		if (stamina_ >= staminaJumpCost_)
		{
			velocity_.y = jumpPower_;
			isGrounded_ = false;
			stamina_ -= staminaJumpCost_;
			isStaminaRecoverBlocked_ = true;
			staminaRecoverTimer_ = 0.0f;
		}
	}

	// 重力を加える
	velocity_.y += gravity_ * deltaTime;
	position.y += velocity_.y; // Y座標に反映

	// 地面との接地処理（仮にY=0が地面）
	if (position.y <= 0.0f)
	{
		position.y = 0.0f;
		velocity_.y = 0.0f;
		isGrounded_ = true;
	}

	animationModel_->SetTranslate(position); // 最終位置反映
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

	if (isAiming_)
	{
		// カメラ中心から照準に向けて発射（ADSモード）
		Vector3 cameraOrigin = camera_->GetTranslate();
		worldMuzzlePos = cameraOrigin;
		Vector3 targetPos = cameraOrigin + camera_->GetForward() * range;
		forward = Vector3::Normalize(targetPos - worldMuzzlePos);
	}
	else
	{
		// 通常時は右手（モデル上の銃口）から発射
		Vector3 localMuzzleOffset = { 2.0f, 1.65f, 5.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		Vector3 rotation = { 0.0f,animationModel_->GetRotate().y, 0.0f };
		Vector3 translation = animationModel_->GetTranslate();
		Matrix4x4 modelMatrix = Matrix4x4::MakeAffineMatrix(scale, rotation, translation);
		worldMuzzlePos = Vector3::Transform(localMuzzleOffset, modelMatrix);
		Vector3 cameraOrigin = camera_->GetTranslate();
		Vector3 targetPos = cameraOrigin + camera_->GetForward() * range;
		forward = Vector3::Normalize(targetPos - worldMuzzlePos);
	}

	weapons_[currentWeaponIndex_]->TryFire(worldMuzzlePos, forward);
}

void Player::UpdateStamina()
{
	// スタミナ回復ブロック中の経過時間を計測
	if (isStaminaRecoverBlocked_)
	{
		staminaRecoverTimer_ += deltaTime;

		// 一定時間経過で回復可能に
		if (staminaRecoverTimer_ >= staminaRecoverDelay_)
		{
			isStaminaRecoverBlocked_ = false;
			staminaRecoverTimer_ = 0.0f;
		}
	}

	// 回復が許可されていればスタミナを回復
	if (!isStaminaRecoverBlocked_)
	{
		stamina_ += staminaRegenRate_ * deltaTime;
		if (stamina_ > maxStamina_)
		{
			stamina_ = maxStamina_;
		}
	}
}
