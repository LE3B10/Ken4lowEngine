#include "MidRangeEnemy.h"
#include <BulletManager.h>
#include <Bullet.h>
#include <CollisionTypeIdDef.h>
#include <fstream>
#include <json.hpp>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	float LengthXZ(const Vector3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }
	Vector3 NormalizeXZ(const Vector3& v) { float l = LengthXZ(v); return l > 0.0001f ? Vector3{ v.x / l, 0.0f, v.z / l } : Vector3{ 0.0f,0.0f,1.0f }; }
}

void MidRangeEnemy::Initialize()
{
	// 追加: 中距離敵の初期化
	EnemyBase::Initialize();
	SetMaxHp(120);
	LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult);
}

void MidRangeEnemy::Update(float deltaTime)
{
	if (IsDead()) { actionState_ = ActionState::DeadAction; }
	if (cooldownTimer_ > 0.0f) cooldownTimer_ -= deltaTime;
	UpdateAction(deltaTime);
	EnemyBase::Update(deltaTime);
}

void MidRangeEnemy::UpdateAction(float deltaTime)
{
	if (!target_ || IsDead()) return;
	const Vector3 toTarget = target_->GetCenterPosition() - GetCenterPosition();
	const float distance = LengthXZ(toTarget);
	if (distance > distanceSettings_.detectRange) { actionState_ = ActionState::CombatIdleAction; SetVelocity({0,GetVelocity().y,0}); return; }
	if (distance < distanceSettings_.tooCloseDistance) actionState_ = ActionState::RetreatAction;
	else if (distance > distanceSettings_.attackMaxRange || distance > distanceSettings_.resumeChaseDistance) actionState_ = ActionState::ChaseTargetAction;
	else if (distance >= distanceSettings_.attackMinRange && distance <= distanceSettings_.attackMaxRange) actionState_ = ActionState::MidRangeAttackAction;
	else actionState_ = ActionState::KeepDistanceAction;
	UpdateMoveAndFacing(deltaTime, toTarget, distance);
	UpdateAttack(deltaTime, toTarget, distance);
}

void MidRangeEnemy::UpdateMoveAndFacing(float deltaTime, const Vector3& toTarget, float)
{
	const Vector3 dir = NormalizeXZ(toTarget);
	Vector3 vel{0.0f, GetVelocity().y, 0.0f};
	if (actionState_ == ActionState::ChaseTargetAction) vel = { dir.x * moveSettings_.moveSpeed, GetVelocity().y, dir.z * moveSettings_.moveSpeed };
	if (actionState_ == ActionState::RetreatAction) vel = { -dir.x * moveSettings_.retreatSpeed, GetVelocity().y, -dir.z * moveSettings_.retreatSpeed };
	if (actionState_ == ActionState::KeepDistanceAction) vel = {0.0f, GetVelocity().y, 0.0f};
	if (actionState_ == ActionState::MidRangeAttackAction) vel = {0.0f, GetVelocity().y, 0.0f};
	SetVelocity(vel);
	yaw_ = std::atan2(dir.x, dir.z);
	SetOrientation({0.0f, yaw_, 0.0f});
	UpdateAnimation(deltaTime, LengthXZ(vel) > 0.1f, castTimer_ > 0.0f);
	UpdateHeadLook(deltaTime, toTarget);
}

void MidRangeEnemy::UpdateAttack(float deltaTime, const Vector3& toTarget, float distance)
{
	if (actionState_ != ActionState::MidRangeAttackAction) { castTimer_ = 0.0f; return; }
	if (distance < distanceSettings_.attackMinRange || distance > distanceSettings_.attackMaxRange) return;
	if (cooldownTimer_ > 0.0f) return;
	castTimer_ += deltaTime;
	if (castTimer_ >= attackSettings_.castTime)
	{
		// 追加: 中距離弾を発射
		FireProjectile(toTarget);
		castTimer_ = 0.0f;
		cooldownTimer_ = attackSettings_.cooldown;
	}
}

void MidRangeEnemy::FireProjectile(const Vector3& toTarget)
{
	if (!bulletManager_) return;
	const Vector3 dir = NormalizeXZ(toTarget);
	// 既存のBulletManager::Spawn APIに合わせて中距離敵弾を生成する。
	Vector3 muzzle = GetCenterPosition() + Vector3{ 0.0f, 1.2f, 0.0f };
	muzzle = muzzle + dir * 1.0f;
	bulletManager_->Spawn(
		muzzle,
		dir,
		attackSettings_.projectileSpeed,
		attackSettings_.damage,
		attackSettings_.projectileLifeTime,
		GetCenterPosition(),
		GetUniqueID(),
		static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet));
}

void MidRangeEnemy::DrawImGui()
{
	EnemyBase::DrawImGui();
#ifdef USE_IMGUI
	if (ImGui::TreeNode("中距離雑魚敵"))
	{
		if (ImGui::Button("読み込み")) { LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult); }
		ImGui::SameLine();
		if (ImGui::Button("保存")) { SaveTuningToJson(tuningIo_.jsonPath, &tuningIo_.lastSaveResult); }
		ImGui::Separator();
		ImGui::SliderInt("最大HP", &maxHp_, 1, 500);
		ImGui::SliderFloat("検知範囲", &distanceSettings_.detectRange, 1.0f, 50.0f);
		ImGui::SliderFloat("攻撃最小距離", &distanceSettings_.attackMinRange, 1.0f, 20.0f);
		ImGui::SliderFloat("攻撃最大距離", &distanceSettings_.attackMaxRange, 1.0f, 30.0f);
		ImGui::SliderFloat("理想距離", &distanceSettings_.keepDistance, 1.0f, 20.0f);
		ImGui::SliderFloat("近すぎる距離", &distanceSettings_.tooCloseDistance, 1.0f, 10.0f);
		ImGui::SliderFloat("移動速度", &moveSettings_.moveSpeed, 0.1f, 10.0f);
		ImGui::SliderFloat("後退速度", &moveSettings_.retreatSpeed, 0.1f, 10.0f);
		ImGui::SliderFloat("回転速度", &moveSettings_.rotateSpeed, 1.0f, 20.0f);
		ImGui::SliderFloat("攻撃クールダウン", &attackSettings_.cooldown, 0.1f, 5.0f);
		ImGui::SliderFloat("構え時間", &attackSettings_.castTime, 0.05f, 3.0f);
		ImGui::SliderFloat("弾速", &attackSettings_.projectileSpeed, 1.0f, 40.0f);
		ImGui::SliderFloat("弾寿命", &attackSettings_.projectileLifeTime, 0.1f, 10.0f);
		ImGui::SliderInt("ダメージ", &attackSettings_.damage, 1, 200);
		ImGui::Text("状態表示: %s", GetCurrentBehaviorName());
		ImGui::Text("読み込み結果: %s", tuningIo_.lastLoadResult.c_str());
		ImGui::Text("保存結果: %s", tuningIo_.lastSaveResult.c_str());
		ImGui::TreePop();
	}
#endif
}

const char* MidRangeEnemy::GetCurrentBehaviorName() const
{
	switch (actionState_) { case ActionState::ChaseTargetAction: return "ChaseTargetAction"; case ActionState::KeepDistanceAction: return "KeepDistanceAction"; case ActionState::RetreatAction: return "RetreatAction"; case ActionState::MidRangeAttackAction: return "MidRangeAttackAction"; case ActionState::CombatIdleAction: return "CombatIdleAction"; case ActionState::DeadAction: return "DeadAction"; default: return "Unknown"; }
}

void MidRangeEnemy::TakeDamage(int amount) { EnemyBase::TakeDamage(amount); }
void MidRangeEnemy::UpdateAnimation(float, bool, bool) {}
void MidRangeEnemy::UpdateHeadLook(float, const Vector3&) {}

bool MidRangeEnemy::LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage)
{
	std::ifstream ifs(path); if (!ifs.is_open()) { if (outMessage) *outMessage = "ファイルなし"; return false; }
	nlohmann::json j; ifs >> j;
	distanceSettings_.detectRange = j["distance"].value("detectRange", distanceSettings_.detectRange);
	distanceSettings_.attackMinRange = j["distance"].value("attackMinRange", distanceSettings_.attackMinRange);
	distanceSettings_.attackMaxRange = j["distance"].value("attackMaxRange", distanceSettings_.attackMaxRange);
	distanceSettings_.keepDistance = j["distance"].value("keepDistance", distanceSettings_.keepDistance);
	distanceSettings_.tooCloseDistance = j["distance"].value("tooCloseDistance", distanceSettings_.tooCloseDistance);
	moveSettings_.moveSpeed = j["move"].value("moveSpeed", moveSettings_.moveSpeed);
	moveSettings_.retreatSpeed = j["move"].value("retreatSpeed", moveSettings_.retreatSpeed);
	attackSettings_.cooldown = j["attack"].value("cooldown", attackSettings_.cooldown);
	attackSettings_.castTime = j["attack"].value("castTime", attackSettings_.castTime);
	attackSettings_.projectileSpeed = j["attack"].value("projectileSpeed", attackSettings_.projectileSpeed);
	attackSettings_.projectileLifeTime = j["attack"].value("projectileLifeTime", attackSettings_.projectileLifeTime);
	attackSettings_.damage = j["attack"].value("damage", attackSettings_.damage);
	if (outMessage) *outMessage = "読み込み成功";
	return true;
}

bool MidRangeEnemy::SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage) const
{
	nlohmann::json j;
	j["basicStats"] = { {"maxHp", maxHp_} };
	j["distance"] = {{"detectRange", distanceSettings_.detectRange},{"attackMinRange", distanceSettings_.attackMinRange},{"attackMaxRange", distanceSettings_.attackMaxRange},{"keepDistance", distanceSettings_.keepDistance},{"tooCloseDistance", distanceSettings_.tooCloseDistance},{"resumeChaseDistance", distanceSettings_.resumeChaseDistance}};
	j["move"] = {{"moveSpeed", moveSettings_.moveSpeed},{"retreatSpeed", moveSettings_.retreatSpeed},{"rotateSpeed", moveSettings_.rotateSpeed}};
	j["attack"] = {{"cooldown", attackSettings_.cooldown},{"castTime", attackSettings_.castTime},{"projectileSpeed", attackSettings_.projectileSpeed},{"projectileLifeTime", attackSettings_.projectileLifeTime},{"damage", attackSettings_.damage},{"attackRadius", attackSettings_.attackRadius}};
	std::filesystem::create_directories(path.parent_path());
	std::ofstream ofs(path);
	ofs << j.dump(2);
	if (outMessage) *outMessage = "保存成功";
	return true;
}
