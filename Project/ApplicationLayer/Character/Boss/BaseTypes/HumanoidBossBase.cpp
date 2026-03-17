#define NOMINMAX
#include "HumanoidBossBase.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	float Clamp(float v, float minValue, float maxValue)
	{
		return (v < minValue) ? minValue : (v > maxValue ? maxValue : v);
	}

	float WrapAngle(float angle)
	{
		while (angle > 3.14159265f) { angle -= 6.28318530f; }
		while (angle < -3.14159265f) { angle += 6.28318530f; }
		return angle;
	}
}

/// -------------------------------------------------------------
/// 人型部位構築
///
/// body_ を本体とし、各部位を body の子としてぶら下げる
/// ここは HumanoidBossBase の中核責務なのでそのまま残す
/// -------------------------------------------------------------
void HumanoidBossBase::BuildBossParts()
{
	GetBody().object.reset();
	GetBodyParts().clear();

	// ---------------------------------------------------------
	// 胴体
	// ---------------------------------------------------------
	{
		auto& body = GetBody();
		body.object = std::make_unique<Object3D>();
		body.object->Initialize(GetBodyModelPath());

		body.transform.translate_ = GetInitialBodyPosition();
		body.transform.rotate_ = { 0.0f, 0.0f, 0.0f };

		body.object->SetTranslate(body.transform.translate_);
		body.object->SetRotate(body.transform.rotate_);
		body.object->SetScale(GetInitialBodyScale());
	}

	// ---------------------------------------------------------
	// 子部位追加ラムダ
	// ---------------------------------------------------------
	auto AddPart =
		[this](const std::string& modelPath,
			const Vector3& localOffset,
			const Vector3& scale)
		{
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

	// BaseCharacter 側の PartIndices 前提:
	// head=0, leftArm=1, rightArm=2, leftLeg=3, rightLeg=4
	AddPart(GetHeadModelPath(), GetHeadLocalOffset(), GetHeadScale());
	AddPart(GetLeftArmModelPath(), GetLeftArmLocalOffset(), GetArmScale());
	AddPart(GetRightArmModelPath(), GetRightArmLocalOffset(), GetArmScale());
	AddPart(GetLeftLegModelPath(), GetLeftLegLocalOffset(), GetLegScale());
	AddPart(GetRightLegModelPath(), GetRightLegLocalOffset(), GetLegScale());
}

/// -------------------------------------------------------------
/// 人型ボス共通設定
///
/// ここでは最低限の初期状態だけ整える
/// 細かい戦闘ロジックや状態開始は派生クラス側で調整する
/// -------------------------------------------------------------
void HumanoidBossBase::SetupBoss()
{
	// ---------------------------------------------------------
	// BossBase 側の状態値とフェーズだけ軽く整える
	// 実際の開始状態は派生側で StateMachine 経由に合わせる
	// ---------------------------------------------------------
	SetState(BossState::Idle);
	SetPhase(BossPhase::Phase1);

	// ---------------------------------------------------------
	// AnimationComponent を使う方針なので、
	// 旧 walkAnimTime / attackAnimTime 系の初期化は持たない
	// 必要な見た目リセットだけ行う
	// ---------------------------------------------------------
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetWalkTimer();
		GetAnimationComponent()->ResetAttackTimer();
		GetAnimationComponent()->ResetAllPose(1.0f);
	}
}

/// -------------------------------------------------------------
/// 衝突
/// 現段階では空実装
/// -------------------------------------------------------------
void HumanoidBossBase::OnCollision(Collider* other)
{
	(void)other;
}

/// -------------------------------------------------------------
/// 状態更新
///
/// HumanoidBossBase ではもう状態ロジックを持たない
/// GuardianBoss などの派生側が担当する
/// -------------------------------------------------------------
void HumanoidBossBase::UpdateState(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
/// 移動更新
///
/// HumanoidBossBase ではもう移動ロジックを持たない
/// 派生ボスまたは BossMovementComponent 側へ任せる
/// -------------------------------------------------------------
void HumanoidBossBase::UpdateMovement(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
/// 攻撃更新
///
/// HumanoidBossBase ではもう攻撃ロジックを持たない
/// 派生ボスまたは BossAttackComponent 側へ任せる
/// -------------------------------------------------------------
void HumanoidBossBase::UpdateAttack(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
/// 死亡チェック
///
/// 共通の死亡判定は BossBase 側にあるため、
/// ここでは基底の処理をそのまま使う
/// -------------------------------------------------------------
void HumanoidBossBase::CheckDeath()
{
	BossBase::CheckDeath();
}

/// -------------------------------------------------------------
/// ターゲットへ向く
///
/// 人型ボス共通で使いやすい補助関数として残す
/// -------------------------------------------------------------
void HumanoidBossBase::FaceTarget(float deltaTime, float rotateSpeed)
{
	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float desiredYaw = std::atan2(toTarget.x, toTarget.z);
	float currentYaw = GetYaw();

	float diff = WrapAngle(desiredYaw - currentYaw);
	const float step = rotateSpeed * deltaTime;
	diff = Clamp(diff, -step, step);

	currentYaw += diff;
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
	ImGui::Begin("HumanoidBoss Debug");

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