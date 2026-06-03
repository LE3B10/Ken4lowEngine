#define NOMINMAX
#include "HumanoidBossBase.h"
#include <LinearInterpolation.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///							人型部位構築
/// -------------------------------------------------------------
void HumanoidBossBase::BuildBossParts()
{
	// ボディと部位の初期化
	GetBody().object.reset();
	GetBodyParts().clear();

	// 胴体
	auto& body = GetBody();
	body.object = std::make_unique<Object3D>();
	body.object->Initialize(GetBodyModelPath());

	body.transform.translate_ = GetInitialBodyPosition();
	body.transform.rotate_ = { 0.0f, 0.0f, 0.0f };

	body.object->SetTranslate(body.transform.translate_);
	body.object->SetRotate(body.transform.rotate_);
	body.object->SetScale(GetInitialBodyScale());

	// 子部位追加ラムダ
	auto AddPart = [this](const std::string& modelPath, const Vector3& localOffset, const Vector3& scale) {
		BaseCharacter::BodyPart part{};

		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);

		part.transform.translate_ = localOffset;
		part.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		part.transform.parent_ = &GetBody().transform;

		part.object->SetTranslate(localOffset);
		part.object->SetRotate(part.transform.rotate_);
		part.object->SetScale(scale);

		GetBodyParts().push_back(std::move(part));
		};

	// 頭と四肢を追加
	AddPart(GetHeadModelPath(), GetHeadLocalOffset(), GetHeadScale());		  // 頭
	AddPart(GetLeftArmModelPath(), GetLeftArmLocalOffset(), GetArmScale());	  // 左腕
	AddPart(GetRightArmModelPath(), GetRightArmLocalOffset(), GetArmScale()); // 右腕
	AddPart(GetLeftLegModelPath(), GetLeftLegLocalOffset(), GetLegScale());	  // 左脚
	AddPart(GetRightLegModelPath(), GetRightLegLocalOffset(), GetLegScale()); // 右脚
}

/// -------------------------------------------------------------
///						人型ボス共通設定
/// -------------------------------------------------------------
void HumanoidBossBase::SetupBoss()
{
	// 状態とフェーズの初期化
	SetState(BossState::Idle);
	SetPhase(BossPhase::Phase1);

	// ターゲット初期化
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetWalkTimer();	 // 歩行アニメ時間リセット
		GetAnimationComponent()->ResetAttackTimer(); // 攻撃アニメ時間リセット
		GetAnimationComponent()->ResetAllPose(1.0f); // 全部位を初期ポーズにリセット
	}
}

/// -------------------------------------------------------------
///								衝突
/// -------------------------------------------------------------
void HumanoidBossBase::OnCollision(Collider* other)
{
	(void)other;
}

/// -------------------------------------------------------------
///							状態更新
/// -------------------------------------------------------------
void HumanoidBossBase::UpdateState(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///							移動更新
/// -------------------------------------------------------------
void HumanoidBossBase::UpdateMovement(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///							攻撃更新
/// -------------------------------------------------------------
void HumanoidBossBase::UpdateAttack(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///						 死亡チェック
/// -------------------------------------------------------------
void HumanoidBossBase::CheckDeath()
{
	// 基底の死亡処理を呼び出す
	BossBase::CheckDeath();
}

/// -------------------------------------------------------------
///						ターゲットへ向く
/// -------------------------------------------------------------
void HumanoidBossBase::FaceTarget(float deltaTime, float rotateSpeed)
{
	// ターゲットへのベクトルをXZ平面で計算
	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	// ベクトルの長さがほとんどない場合は回転しない
	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f) return;
	
	// 目標の向きを計算
	const float desiredYaw = std::atan2(toTarget.x, toTarget.z);
	float currentYaw = GetYaw();

	// 角度の差を求め、回転速度に基づいて補間
	float diff = WrapAngle(desiredYaw - currentYaw);
	const float step = rotateSpeed * deltaTime;

	// 角度の差を回転速度で制限
	diff = std::clamp(diff, -step, step);

	// 現在の向きを更新
	currentYaw += diff;

	// 更新した向きをセット
	SetYaw(currentYaw);
}

/// -------------------------------------------------------------
/// XZ距離
/// -------------------------------------------------------------
float HumanoidBossBase::GetDistanceToTargetXZ() const
{
	const Vector3 pos = GetPosition();
	const Vector3 target = GetTargetPosition();

	const float dx = target.x - pos.x;
	const float dz = target.z - pos.z;
	return std::sqrt(dx * dx + dz * dz);
}

/// -------------------------------------------------------------
/// ImGui
///
/// 人型共通の見た目確認用だけ残す
/// 旧 stateTimer / attackCooldownTimer などの内部状態表示は外す
/// -------------------------------------------------------------
void HumanoidBossBase::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("HumanoidBoss Debug"))
	{
		ImGui::End();
		return;
	}

	const Vector3 center = GetCenterPosition();
	ImGui::Text("Center : (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
	ImGui::Text("HP     : %.1f / %.1f", GetHP(), GetMaxHP());
	ImGui::Text("Yaw    : %.2f", GetYaw());
	ImGui::Text("DistXZ : %.2f", GetDistanceToTargetXZ());

	auto& body = GetBody();
	if (ImGui::CollapsingHeader("Body", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Body Pos", &body.transform.translate_.x, 0.01f);
		ImGui::DragFloat3("Body Rot", &body.transform.rotate_.x, 0.01f);
	}

	auto& parts = GetBodyParts();
	const auto& idx = GetPartIndices();

	if (idx.head < parts.size() &&
		ImGui::CollapsingHeader("Head", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Head Local Pos", &parts[idx.head].transform.translate_.x, 0.01f);
		ImGui::DragFloat3("Head Local Rot", &parts[idx.head].transform.rotate_.x, 0.01f);
	}

	if (idx.leftArm < parts.size() &&
		ImGui::CollapsingHeader("Left Arm", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("LArm Local Pos", &parts[idx.leftArm].transform.translate_.x, 0.01f);
		ImGui::DragFloat3("LArm Local Rot", &parts[idx.leftArm].transform.rotate_.x, 0.01f);
	}

	if (idx.rightArm < parts.size() &&
		ImGui::CollapsingHeader("Right Arm", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("RArm Local Pos", &parts[idx.rightArm].transform.translate_.x, 0.01f);
		ImGui::DragFloat3("RArm Local Rot", &parts[idx.rightArm].transform.rotate_.x, 0.01f);
	}

	if (idx.leftLeg < parts.size() &&
		ImGui::CollapsingHeader("Left Leg", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("LLeg Local Pos", &parts[idx.leftLeg].transform.translate_.x, 0.01f);
		ImGui::DragFloat3("LLeg Local Rot", &parts[idx.leftLeg].transform.rotate_.x, 0.01f);
	}

	if (idx.rightLeg < parts.size() &&
		ImGui::CollapsingHeader("Right Leg", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("RLeg Local Pos", &parts[idx.rightLeg].transform.translate_.x, 0.01f);
		ImGui::DragFloat3("RLeg Local Rot", &parts[idx.rightLeg].transform.rotate_.x, 0.01f);
	}

	ImGui::End();
#endif
}