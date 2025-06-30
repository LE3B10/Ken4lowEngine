#include "Boss.h"
#include <LogString.h>
#include "CollisionTypeIdDef.h"
#include "CollisionManager.h"
#include "Player.h"

#include <imgui.h>

void Boss::Initialize()
{
	stateModelFiles_[BossState::Idle] = "BossModel/BossIdleModel.gltf";
	stateModelFiles_[BossState::Chase] = "BossModel/BossWalkModel.gltf";
	stateModelFiles_[BossState::Melee] = "BossModel/BossMeleeModel.gltf";
	stateModelFiles_[BossState::Shoot] = "BossModel/BossShootModel.gltf";
	stateModelFiles_[BossState::Dead] = "BossModel/BossDyingModel.gltf";

	// 各モデルをロードしてマップに保存
	for (const auto& [state, path] : stateModelFiles_) {
		auto m = std::make_unique<AnimationModel>();
		m->SetScaleFactor(scaleFactor_);
		m->Initialize(path);
		models_[state] = std::move(m);
	}

	// 最初のモデルをセット
	model_ = models_[BossState::Idle].get();
	model_->SetTranslate({ 0.0f, 0.0f, 10.0f });
}

void Boss::Update()
{
	if (!player_) return;

	// 共通更新
	model_->Update();

	// プレイヤーとの距離計算
	Vector3 toPlayer = player_->GetCenterPosition() - model_->GetTranslate();
	float distance = Vector3::Length(toPlayer);

	// 射撃クールダウン更新
	if (shootCooldown_ > 0.0f) {
		shootCooldown_ -= deltaTime_;
	}

	switch (state_)
	{
	case BossState::Idle:
		if (distance < 50.0f) { // ←広めに変更
			ChangeState(BossState::Chase);
		}
		break;

	case BossState::Chase:
	{
		// 移動／攻撃の距離判断
		if (distance > 10.0f && shootCooldown_ <= 0.0f) {
			ChangeState(BossState::Shoot);
		}
		else if (distance < 2.5f && meleeCooldown_ <= 0.0f) {
			ChangeState(BossState::Melee);
		}
		else {
			// 回転は自分の位置を中心とした方向ベクトル
			Vector3 toPlayer = player_->GetCenterPosition() - model_->GetTranslate();
			Vector3 dir = Vector3::Normalize(toPlayer);

			float targetYaw = atan2f(dir.x, dir.z); // Z+前提
			float currentYaw = model_->GetRotate().y;

			float deltaYaw = targetYaw - currentYaw;

			// -π～π に補正
			if (deltaYaw > std::numbers::pi_v<float>) deltaYaw -= std::numbers::pi_v<float> *2.0f;
			if (deltaYaw < -std::numbers::pi_v<float>) deltaYaw += std::numbers::pi_v<float> *2.0f;

			float rotateSpeed = 4.0f * deltaTime_;
			float newYaw = currentYaw + deltaYaw * rotateSpeed;

			model_->SetRotate({ 0, newYaw, 0 });

			Vector3 forward = { sinf(newYaw), 0, cosf(newYaw) };
			Vector3 newPos = model_->GetTranslate() + forward * 0.1f;

			model_->SetTranslate(newPos);
		}
	}
	break;


	case BossState::Shoot:
		shootDuration_ += deltaTime_;
		if (!didShoot_) {
			Log("Boss performs Shoot attack!");
			didShoot_ = true;
		}

		if (shootDuration_ >= shootMaxDuration_) {
			shootCooldown_ = shootCooldownMax_; // ←追加
			ChangeState(BossState::Chase);
		}
		break;

	case BossState::Melee:
		meleeDuration_ += deltaTime_;
		if (!didMelee_) {
			Log("Boss performs Melee attack!");
			didMelee_ = true;
		}

		if (meleeDuration_ >= meleeMaxDuration_) {
			meleeCooldown_ = 1.0f;
			ChangeState(BossState::Idle);
		}
		break;
	}

	// 毎フレームカプセルを骨に追従
	auto partCaps = model_->GetBodyPartCapsulesWorld();

	if (bodyCols_.size() != partCaps.size()) {
		// Bone 本数が変わったら作り直し（開発中のモデル差し替え対策）
		bodyCols_.clear();
		for (auto& [name, cap] : partCaps) {
			auto c = std::make_unique<Collider>();
			c->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
			c->SetCapsule(cap);
			bodyCols_.push_back({ name, std::move(c) });
		}
	}

	for (size_t i = 0; i < partCaps.size(); ++i) {
		bodyCols_[i].col->SetCapsule(partCaps[i].second);
	}
}

void Boss::Draw()
{
	if (isDead_) return;

	model_->Draw();
	model_->DrawSkeletonWireframe();
	for (auto& pc : bodyCols_) { pc.col->Draw(); }
}

void Boss::DrawImGui()
{
	model_->DrawImGui();

	ImGui::Begin("Boss Colliders");
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

	// --- ステート切り替え用 UI ---
	ImGui::Begin("Boss Debug");

	static int currentState = static_cast<int>(state_);
	const char* stateNames[] = { "Idle", "Chase", "Melee", "Shoot", "Special", "Dead" };

	// ドロップダウンから選択
	if (ImGui::Combo("State", &currentState, stateNames, IM_ARRAYSIZE(stateNames))) {
		ChangeState(static_cast<BossState>(currentState));
	}

	ImGui::End();
}

void Boss::RegisterColliders(CollisionManager* collisionManager) const
{
	// 本体（必要なら）も先に登録
	collisionManager->AddCollider(const_cast<Boss*>(this));

	// 部位カプセルをすべて登録
	for (const auto& pc : bodyCols_) {
		collisionManager->AddCollider(pc.col.get());
	}
}

void Boss::OnCollision(Collider* other)
{

}

void Boss::TakeDamage(float damage)
{
	if (isDead_) return;

	hp_ -= damage;
	if (hp_ <= 0.0f)
	{
		isDead_ = true;
		hp_ = 0.0f;
		Log("Boss is dead");
	}
}

void Boss::ChangeState(BossState newState)
{
	if (state_ == newState) return;

	// 攻撃状態に入るたびにフラグをリセット
	didShoot_ = false;
	didMelee_ = false;
	shootDuration_ = 0.0f;
	meleeDuration_ = 0.0f;

	// 旧モデルが null でない場合だけ位置を保存
	Vector3 currentPos = model_ ? model_->GetTranslate() : Vector3(0, 0, 0);
	Vector3 currentRot = model_ ? model_->GetRotate() : Vector3(0, 0, 0);

	state_ = newState;

	if (models_.contains(state_) && models_[state_])
	{
		model_ = models_[state_].get();
		model_->SetScaleFactor(scaleFactor_);
		model_->SetTranslate(currentPos);  // 座標を確実に復元
		model_->SetRotate(currentRot);
	}
}