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
	model_->SetTranslate({ 0.0f, 0.0f, 1.0f });
}

void Boss::Update()
{
	if (!isDying_ && !isDead_)
	{
		// 共通更新
		model_->Update();

		// 毎フレームカプセルを骨に追従
		auto partCaps = model_->GetBodyPartCapsulesWorld();

		if (bodyCols_.size() != partCaps.size()) {
			// Bone 本数が変わったら作り直し（開発中のモデル差し替え対策）
			bodyCols_.clear();
			for (auto& [name, cap] : partCaps) {
				auto c = std::make_unique<Collider>();
				c->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBoss));
				c->SetCapsule(cap);
				c->SetOwner(this);
				bodyCols_.push_back({ name, std::move(c) });
			}
		}

		for (size_t i = 0; i < partCaps.size(); ++i) {
			bodyCols_[i].col->SetCapsule(partCaps[i].second);
		}
	}

	if (isDying_)
	{
		model_->Update();
		deathTime_ += deltaTime_;

		// アニメが終わる
		if (deathTime_ >= kDeathDuration_)
		{
			isDying_ = false;
			isDissolving_ = true;
			dissolveTime_ = 0.0f;

			// 現在のポーズで固定（再生停止）
			model_->SetIsPlaying(false);

			model_->SetDissolveThreshold(0.0f); // 初期化
		}
		return;
	}

	if (isDissolving_)
	{
		dissolveTime_ += deltaTime_;
		float t = std::clamp(dissolveTime_ / dissolveDuration_, 0.0f, 1.0f);
		model_->SetDissolveThreshold(t);

		if (t >= 1.0f) {
			isDead_ = true; // 完全に消えた後にフラグを立てる
		}
	}
}

void Boss::Draw()
{
	if (isDead_) return;  // 死亡している場合は描画しない
	model_->Draw();

	if (!isDying_) {
		// デバッグ表示は演出中は不要ならスキップ
		model_->DrawSkeletonWireframe();
		for (auto& pc : bodyCols_) pc.col->Draw();
	}
}

void Boss::DrawImGui()
{
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

	// HP
	ImGui::Text("Boss HP : %f", hp_);

	model_->DrawImGui();

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

	if (isDead_)
	{
		for (const auto& pc : bodyCols_) {
			collisionManager->RemoveCollider(pc.col.get());
		}
	}
}

void Boss::OnCollision(Collider* other)
{

}

void Boss::TakeDamage(float damage)
{
	if (isDead_ || isDying_) return;          // 既に死亡演出中なら無視

	hp_ -= damage;
	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;
		Log("Boss is dead");
	}

	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;

		// 死亡演出
		isDying_ = true;
		deathTime_ = 0.0f;
		ChangeState(BossState::Dead);
		Log("Boss is dying...");

		// 弾とぶつからないようコライダーを無効化
	}
}

void Boss::ChangeState(BossState newState)
{
	// 状態に応じてアニメーション時間をリセットするかどうか
	bool resetAnimTime = (newState == BossState::Dead || newState == BossState::Melee || newState == BossState::Shoot);

	// アニメーションの時間を保存または0に初期化
	float newAnimTime = (!resetAnimTime && model_) ? model_->GetAnimationTime() : 0.0f;

	// 現在の位置・回転を保存
	Vector3 currentPos = model_ ? model_->GetTranslate() : Vector3(0, 0, 0);
	Vector3 currentRot = model_ ? model_->GetRotate() : Vector3(0, 0, 0);

	state_ = newState;

	if (models_.contains(state_) && models_[state_])
	{
		model_ = models_[state_].get();
		model_->SetScaleFactor(scaleFactor_);
		model_->SetTranslate(currentPos);
		model_->SetRotate(currentRot);

		// 状態に応じて時間を初期化 or 継続
		model_->SetAnimationTime(newAnimTime);

		// 安全のためアニメ・ディゾルブ初期化
		model_->SetDissolveThreshold(0.0f);
		model_->SetIsPlaying(true);

		model_->SetDissolveThreshold(0.0f);
		model_->Update(); // ←ボーンとマテリアル情報を即時更新
	}
}