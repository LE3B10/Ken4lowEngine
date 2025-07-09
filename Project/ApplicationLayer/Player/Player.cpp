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
#include <CollisionManager.h>

#include <imgui.h>
#include <PostEffectManager.h>

#include "PlayerIdleState.h"


/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();

	// アニメーションモデルの初期化
	animationModel_ = std::make_unique<AnimationModel>();

	ChangeState(std::make_unique<PlayerIdleState>());  // Idle で開始

	// 武器の初期化
	InitializeWeapons();

	for (auto& weapon : weapons_)
	{
		weapon->SetPlayer(this); // プレイヤーを武器に設定
	}

	// プレイヤーコントローラーの生成と初期化
	controller_ = std::make_unique<PlayerController>();
	controller_->Initialize(animationModel_.get());
	controller_->SetStaminaPointer(&stamina_); // ←★ここを追加

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

	weapons_[currentWeaponIndex_]->SetFpsCamera(fpsCamera_);

	// アニメーションモデルの更新
	if (animationModel_) animationModel_->Update();
	// プレイヤーコントローラーの更新
	controller_->UpdateMovement(camera_, animationModel_->GetDeltaTime(), weapons_[currentWeaponIndex_]->IsReloading());

	// プレイヤーの状態更新（例: 歩行、待機など）
	if (state_) state_->Update(this);

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
	if (Weapon* weapon = GetCurrentWeapon()) weapon->Update();

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

	// 毎フレームカプセルを骨に追従
	auto partCaps = animationModel_->GetBodyPartCapsulesWorld();

	if (bodyCols_.size() != partCaps.size()) {
		// Bone 本数が変わったら作り直し（開発中のモデル差し替え対策）
		bodyCols_.clear();
		for (auto& [name, cap] : partCaps) {
			auto c = std::make_unique<Collider>();
			c->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
			c->SetCapsule(cap);
			c->SetOwner(this); // プレイヤーをオーナーに設定
			bodyCols_.push_back({ name, std::move(c) });
		}
	}

	for (size_t i = 0; i < partCaps.size(); ++i) {
		bodyCols_[i].col->SetCapsule(partCaps[i].second);
	}
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

	AnimationPipelineBuilder::GetInstance()->SetRenderSetting(); // アニメーションパイプラインの描画設定
	if (IsDebugCamera()) animationModel_->Draw(); // アニメーションモデルの描画
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

	controller_->DrawMovementImGui(); // コントローラーのImGui描画

	ImGui::Text("Current Weapon: %s", weaponName.c_str());

	ImGui::Begin("Player Colliders");
	static bool visible = false;
	if (ImGui::Checkbox("Show All Capsules", &visible)) {
		for (auto& pc : bodyCols_) {
			pc.col->SetCapsuleVisible(visible);
		}
	}
	for (size_t i = 0; i < bodyCols_.size(); ++i) {
		if (ImGui::TreeNode(bodyCols_[i].name.c_str())) {
			bodyCols_[i].col->DrawImGui();
			ImGui::TreePop();
		}
	}
	ImGui::End();
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
}

void Player::RegisterColliders(CollisionManager* collisionManager) const
{
	// プレイヤーのコライダーを登録
	collisionManager->AddCollider(const_cast<Player*>(this));

	// 部位カプセルをすべて登録
	for (const auto& pc : bodyCols_) {
		collisionManager->AddCollider(pc.col.get());
	}

	// プレイヤーが死亡している場合、コライダーを削除
	if (IsDead()) collisionManager->RemoveCollider(const_cast<Player*>(this));
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

	if (controller_->IsAimingInput())
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

void Player::ChangeState(std::unique_ptr<IPlayerState> next)
{
	if (state_) state_->Finalize(this);
	state_ = std::move(next);
	if (state_) state_->Initialize(this);
}
