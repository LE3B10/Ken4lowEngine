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
#include <PostEffectManager.h>

#include "IdleBehavior.h"
#include "WalkingBehavior.h"
#include "RunningBehavior.h"

#include <imgui.h>


Player::~Player()
{
	//AnimationModelFactory::ClearAll();

	// プレイヤーのデストラクタ
	// ここで必要なクリーンアップ処理を行う
	weapons_.clear(); // 武器のクリア
	controller_.reset(); // プレイヤーコントローラーのリセット
	animationModel_.reset(); // アニメーションモデルのリセット
	numberSpriteDrawer_.reset(); // 数字スプライト描画クラスのリセット
	// 各ビヘイビアのクリア
	for (auto& behavior : behaviors_) {
		behavior.second.reset();
	}
	behaviors_.clear();
}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();

	// 必要なアニメーションモデルを事前ロード
	//AnimationModelFactory::PreLoadModel("PlayerStateModel/human.gltf");
	//AnimationModelFactory::PreLoadModel("PlayerStateModel/humanWalking.gltf");
	//AnimationModelFactory::PreLoadModel("PlayerStateModel/PlayerRunState.gltf");

	//// アニメーションモデルの初期化
	//animationModel_ = AnimationModelFactory::CreateInstance("PlayerStateModel/human.gltf");
	animationModel_->SetScaleFactor(1.0f); // スケールファクターを設定
	animationModel_->InitializeBones(); // ボーン情報の初期化

	currentState_ = ModelState::Idle; // 初期状態をIdleに設定

	// 各ビヘイビア登録
	behaviors_[ModelState::Idle] = std::make_unique<IdleBehavior>();
	behaviors_[ModelState::Walking] = std::make_unique<WalkingBehavior>();
	behaviors_[ModelState::Running] = std::make_unique<RunningBehavior>();
	//behaviors_[ModelState::Jumping] = std::make_unique<JumpingBehavior>();

	// 武器の初期化
	InitializeWeapons();

	for (auto& weapon : weapons_)
	{
		weapon->SetPlayer(this); // プレイヤーを武器に設定
	}

	// プレイヤーコントローラーの生成と初期化
	controller_ = std::make_unique<PlayerController>();
	controller_->Initialize(animationModel_.get());
	controller_->SetStaminaPointer(&stamina_);

	// HUDの初期化
	numberSpriteDrawer_ = std::make_unique<NumberSpriteDrawer>();
	numberSpriteDrawer_->Initialize("number.png", 50.0f, 50.0f);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	if (isDead_) return; // 死亡後は行動不可

	weapons_[currentWeaponIndex_]->SetFpsCamera(fpsCamera_);

	// プレイヤーコントローラーの更新
	controller_->UpdateMovement(camera_, animationModel_->GetDeltaTime(), weapons_[currentWeaponIndex_]->IsReloading());

	// アニメーションモデルの更新
	if (behaviors_.count(currentState_)) {
		behaviors_[currentState_]->Update(this);
	}

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
	if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Primary)
	{
		if (controller_->IsPushShooting())
		{
			FireWeapon();
		}
	}
	else if (GetCurrentWeapon()->GetWeaponType() == WeaponType::Backup)
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
	if (behaviors_.count(currentState_)) {
		if (IsDebugCamera()) behaviors_[currentState_]->Draw(this);
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
	case WeaponType::Primary:
		weaponName = "Rifle";
		break;

	case WeaponType::Backup:
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
	rifle->SetWeaponType(WeaponType::Primary);
	rifle->Initialize();
	weapons_.push_back(std::move(rifle));

	// ショットガンの初期化
	auto shotgun = std::make_unique<Weapon>();
	shotgun->SetWeaponType(WeaponType::Backup);
	shotgun->Initialize();
	weapons_.push_back(std::move(shotgun));
}


/// -------------------------------------------------------------
///				　			　 衝突処理
/// -------------------------------------------------------------
void Player::OnCollision(Collider* other)
{
	// 衝突相手が nullptr の場合は処理をスキップ

	// 衝突相手がプレイヤー自身の場合は処理をスキップ
	UNREFERENCED_PARAMETER(other);
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
	if (isDead_ || amount <= 0) return;
	hp_ = std::min(hp_ + static_cast<float>(amount), maxHP_);
}

void Player::SetState(ModelState state, bool force)
{
	if (currentState_ != state || force)
	{
		auto it = behaviors_.find(state);
		if (it != behaviors_.end())
		{
			currentState_ = state;
			it->second->Initialize(this);
		}
	}
}

void Player::SetAnimationModel(std::shared_ptr<AnimationModel> model)
{
	// 位置・回転・スケールを保持しておく
	if (animationModel_)
	{
		model->SetTranslate(animationModel_->GetTranslate());
		model->SetRotate(animationModel_->GetRotate());
		model->SetScale(animationModel_->GetScale());
	}

	// 新しいモデルに差し替え
	animationModel_ = model;

	if (controller_) {
		controller_->SetAnimationModel(model.get());
	}
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
		// カメラ基準の座標軸
		const Vector3 camPos = camera_->GetTranslate();
		const Vector3 f = Vector3::Normalize(camera_->GetForward());
		const Vector3 up = { 0.0f, 1.0f, 0.0f };
		const Vector3 right = Vector3::Normalize(Vector3::Cross(up, f));

		// --- ここを好みで微調整 ---
		const float side = 0.10f;  // +で画面右側（右手持ち想定）
		const float height = -0.05f; // 少し下げる（画面下方向）
		const float forwardOff = 0.20f;  // 画面奥にオフセット（近距離の自己衝突回避）
		// -------------------------

		// ADSでもカメラ中心から少し右下前に寄せた位置＝銃口的な扱い
		worldMuzzlePos = camPos + right * side + up * height + f * forwardOff;

		// 狙点はカメラ正面の遠点。弾道は「銃口→狙点」のベクトルに
		const Vector3 targetPos = camPos + f * range;
		forward = Vector3::Normalize(targetPos - worldMuzzlePos);
	}
	else
	{
		// 通常時（既存）：モデルの銃口付近 → 狙点
		// ※今の実装でOK。必要なら localMuzzleOffset を微調整
		Vector3 localMuzzleOffset = { 0.0f, 1.65f, 0.0f }; // 好みで調整
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		Vector3 rotation = { 0.0f, animationModel_->GetRotate().y, 0.0f };
		Vector3 translation = animationModel_->GetTranslate();
		Matrix4x4 modelMatrix = Matrix4x4::MakeAffineMatrix(scale, rotation, translation);
		worldMuzzlePos = Vector3::Transform(localMuzzleOffset, modelMatrix);

		const Vector3 camPos = camera_->GetTranslate();
		const Vector3 targetPos = camPos + camera_->GetForward() * range;
		forward = Vector3::Normalize(targetPos - worldMuzzlePos);
	}

	// 最後に発射
	weapons_[currentWeaponIndex_]->TryFire(worldMuzzlePos, forward);
}
