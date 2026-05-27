#include "MidRangeEnemy.h"
#include <Collider.h>
#include <Wireframe.h>
#include <fstream>
#include <cmath>
#include <json.hpp>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	Vector3 NormalizeXZ(const Vector3& v)
	{
		float l = LengthXZ(v);
		return l > 0.0001f ? Vector3{ v.x / l, 0.0f, v.z / l } : Vector3{ 0.0f, 0.0f, 1.0f };
	}
}

void MidRangeEnemy::Initialize()
{
	EnemyBase::Initialize();
	SetMaxHp(120);
	LoadTuningFromJson(jsonPath_, nullptr);
}

void MidRangeEnemy::Update(float deltaTime)
{
	if (cooldownTimer_ > 0.0f) { cooldownTimer_ -= deltaTime; }
	UpdateAction(deltaTime);
	activeBomb_.Update(deltaTime, floorAABBs_, wallObstacleAABBs_);
	if (activeBomb_.ConsumeExplosionEvent())
	{
		// 追加: ダメージ接続先が未確定のため、理由を保持してImGuiで確認可能にする。
		lastThrowReason_ = activeBomb_.GetDebugLastReason();
	}
	EnemyBase::Update(deltaTime);
}

void MidRangeEnemy::UpdateAction(float deltaTime)
{
	if (!target_ || IsDead()) { return; }
	const Vector3 toTarget = target_->GetCenterPosition() - GetCenterPosition();
	const float distance = LengthXZ(toTarget);
	const Vector3 dir = NormalizeXZ(toTarget);

	Vector3 vel = { 0.0f, GetVelocity().y, 0.0f };
	if (distance > bombAttackSettings_.attackMaxRange) { actionState_ = ActionState::Chase; vel = { dir.x * moveSettings_.moveSpeed, GetVelocity().y, dir.z * moveSettings_.moveSpeed }; }
	else if (distance < bombAttackSettings_.tooCloseRange) { actionState_ = ActionState::Retreat; vel = { -dir.x * moveSettings_.retreatSpeed, GetVelocity().y, -dir.z * moveSettings_.retreatSpeed }; }
	else { actionState_ = ActionState::CastBomb; }
	SetVelocity(vel);
	SetOrientation({ 0.0f, std::atan2(dir.x, dir.z), 0.0f });
	UpdateAttack(deltaTime, toTarget, distance);
}

void MidRangeEnemy::UpdateAttack(float deltaTime, const Vector3& toTarget, float distance)
{
	if (actionState_ != ActionState::CastBomb || cooldownTimer_ > 0.0f)
	{
		castTimer_ = 0.0f;
		castingBomb_ = false;
		return;
	}
	if (distance < bombAttackSettings_.attackMinRange || distance > bombAttackSettings_.attackMaxRange || activeBomb_.IsAlive()) { return; }
	castingBomb_ = true;
	castTimer_ += deltaTime;
	if (castTimer_ >= bombAttackSettings_.castTime)
	{
		ThrowBomb(toTarget);
		castTimer_ = 0.0f;
		cooldownTimer_ = bombAttackSettings_.cooldown;
		castingBomb_ = false;
	}
}

void MidRangeEnemy::ThrowBomb(const Vector3& toTarget)
{
	// 追加: MidRangeEnemy専用の爆弾投擲を行う。
	Vector3 startPos = GetCenterPosition();
	startPos.y += bombAttackSettings_.throwHeightOffset;
	activeBomb_.Launch(startPos, startPos + toTarget, bombProjectileSettings_, this, target_);
	lastThrowReason_ = "通常投擲";
}

void MidRangeEnemy::DrawImGui()
{
	EnemyBase::DrawImGui();
#ifdef USE_IMGUI
	if (ImGui::TreeNode("中距離雑魚敵"))
	{
		if (ImGui::Button("読み込み")) { LoadTuningFromJson(jsonPath_, nullptr); }
		ImGui::SameLine();
		if (ImGui::Button("保存")) { SaveTuningToJson(jsonPath_, nullptr); }
		if (ImGui::TreeNode("爆弾攻撃"))
		{
			ImGui::SliderFloat("攻撃最小距離", &bombAttackSettings_.attackMinRange, 1.0f, 30.0f);
			ImGui::SliderFloat("攻撃最大距離", &bombAttackSettings_.attackMaxRange, 1.0f, 30.0f);
			ImGui::SliderFloat("理想距離", &bombAttackSettings_.idealRange, 1.0f, 20.0f);
			ImGui::SliderFloat("近すぎる距離", &bombAttackSettings_.tooCloseRange, 1.0f, 20.0f);
			ImGui::SliderFloat("攻撃クールダウン", &bombAttackSettings_.cooldown, 0.1f, 10.0f);
			ImGui::SliderFloat("構え時間", &bombAttackSettings_.castTime, 0.05f, 5.0f);
			ImGui::SliderFloat("投げ開始高さ", &bombAttackSettings_.throwHeightOffset, 0.1f, 5.0f);
			ImGui::SliderFloat("爆弾初速", &bombProjectileSettings_.initialSpeed, 1.0f, 40.0f);
			ImGui::SliderFloat("爆弾上方向速度", &bombProjectileSettings_.upwardVelocity, 0.1f, 20.0f);
			ImGui::SliderFloat("爆弾重力", &bombProjectileSettings_.gravity, 0.1f, 40.0f);
			ImGui::SliderFloat("爆弾寿命", &bombProjectileSettings_.lifeTime, 0.1f, 10.0f);
			ImGui::SliderFloat("爆発半径", &bombProjectileSettings_.explosionRadius, 0.1f, 10.0f);
			ImGui::SliderInt("直撃ダメージ", &bombProjectileSettings_.directHitDamage, 1, 999);
			ImGui::SliderInt("爆発ダメージ", &bombProjectileSettings_.explosionDamage, 1, 999);
			ImGui::SliderFloat("直撃判定半径", &bombProjectileSettings_.hitRadius, 0.1f, 3.0f);
			ImGui::TreePop();
		}
		ImGui::Text("現在行動: %d", static_cast<int>(actionState_));
		ImGui::Text("攻撃クールダウン残り: %.2f", cooldownTimer_);
		ImGui::Text("構え中か: %s", castingBomb_ ? "はい" : "いいえ");
		ImGui::Text("最後に爆弾を投げた理由: %s", lastThrowReason_.c_str());
		ImGui::Text("現在の爆弾数: %d", activeBomb_.IsAlive() ? 1 : 0);
		ImGui::TreePop();
	}
#endif
	activeBomb_.DrawDebug();
}

void MidRangeEnemy::TakeDamage(int amount) { EnemyBase::TakeDamage(amount); }

bool MidRangeEnemy::LoadTuningFromJson(const std::filesystem::path& path, std::string*)
{
	std::ifstream ifs(path);
	if (!ifs.is_open()) { return false; }
	nlohmann::json j; ifs >> j;
	bombAttackSettings_.attackMinRange = j["bombAttack"].value("attackMinRange", bombAttackSettings_.attackMinRange);
	bombAttackSettings_.attackMaxRange = j["bombAttack"].value("attackMaxRange", bombAttackSettings_.attackMaxRange);
	bombAttackSettings_.idealRange = j["bombAttack"].value("idealRange", bombAttackSettings_.idealRange);
	bombAttackSettings_.tooCloseRange = j["bombAttack"].value("tooCloseRange", bombAttackSettings_.tooCloseRange);
	bombAttackSettings_.cooldown = j["bombAttack"].value("cooldown", bombAttackSettings_.cooldown);
	bombAttackSettings_.castTime = j["bombAttack"].value("castTime", bombAttackSettings_.castTime);
	bombAttackSettings_.throwHeightOffset = j["bombAttack"].value("throwHeightOffset", bombAttackSettings_.throwHeightOffset);
	bombProjectileSettings_.initialSpeed = j["bombProjectile"].value("initialSpeed", bombProjectileSettings_.initialSpeed);
	bombProjectileSettings_.upwardVelocity = j["bombProjectile"].value("upwardVelocity", bombProjectileSettings_.upwardVelocity);
	bombProjectileSettings_.gravity = j["bombProjectile"].value("gravity", bombProjectileSettings_.gravity);
	bombProjectileSettings_.lifeTime = j["bombProjectile"].value("lifeTime", bombProjectileSettings_.lifeTime);
	bombProjectileSettings_.explosionRadius = j["bombProjectile"].value("explosionRadius", bombProjectileSettings_.explosionRadius);
	bombProjectileSettings_.directHitDamage = j["bombProjectile"].value("directHitDamage", bombProjectileSettings_.directHitDamage);
	bombProjectileSettings_.explosionDamage = j["bombProjectile"].value("explosionDamage", bombProjectileSettings_.explosionDamage);
	bombProjectileSettings_.hitRadius = j["bombProjectile"].value("hitRadius", bombProjectileSettings_.hitRadius);
	return true;
}

bool MidRangeEnemy::SaveTuningToJson(const std::filesystem::path& path, std::string*) const
{
	nlohmann::json j;
	j["bombAttack"] = {{"attackMinRange", bombAttackSettings_.attackMinRange},{"attackMaxRange", bombAttackSettings_.attackMaxRange},{"idealRange", bombAttackSettings_.idealRange},{"tooCloseRange", bombAttackSettings_.tooCloseRange},{"cooldown", bombAttackSettings_.cooldown},{"castTime", bombAttackSettings_.castTime},{"throwHeightOffset", bombAttackSettings_.throwHeightOffset}};
	j["bombProjectile"] = {{"initialSpeed", bombProjectileSettings_.initialSpeed},{"upwardVelocity", bombProjectileSettings_.upwardVelocity},{"gravity", bombProjectileSettings_.gravity},{"lifeTime", bombProjectileSettings_.lifeTime},{"explosionRadius", bombProjectileSettings_.explosionRadius},{"directHitDamage", bombProjectileSettings_.directHitDamage},{"explosionDamage", bombProjectileSettings_.explosionDamage},{"hitRadius", bombProjectileSettings_.hitRadius}};
	std::filesystem::create_directories(path.parent_path());
	std::ofstream ofs(path);
	ofs << j.dump(2);
	return true;
}
