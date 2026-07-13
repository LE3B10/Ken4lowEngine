#define NOMINMAX
#include "HumanoidBossBase.h"
#include <LinearInterpolation.h>
#include <LogString.h>
#include <ParameterManager.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kHumanoidBossModelsGroup = "HumanoidBossModels";

	void EnsureHumanoidModelParameters()
	{
		static bool isInitialized = false;
		if (isInitialized)
		{
			return;
		}
		isInitialized = true;

		auto* parameters = ParameterManager::GetInstance();
		parameters->CreateGroup(kHumanoidBossModelsGroup);

		// モデルパスJSONがあれば読み込み、不足分だけ既定モデルで補完する。
		if (std::filesystem::exists("Resources/ParameterManager/HumanoidBossModels.json"))
		{
			parameters->LoadFile(kHumanoidBossModelsGroup);
		}
		else
		{
			Log("[HumanoidBossBase] HumanoidBossModels.json not found. Use built-in default model paths.\n");
		}

		parameters->AddItem(kHumanoidBossModelsGroup, "bodyModelPath", std::string("Characters/body.gltf"));
		parameters->AddItem(kHumanoidBossModelsGroup, "headModelPath", std::string("Characters/head.gltf"));
		parameters->AddItem(kHumanoidBossModelsGroup, "leftArmModelPath", std::string("Characters/left_arm.gltf"));
		parameters->AddItem(kHumanoidBossModelsGroup, "rightArmModelPath", std::string("Characters/right_arm.gltf"));
		parameters->AddItem(kHumanoidBossModelsGroup, "leftLegModelPath", std::string("Characters/left_leg.gltf"));
		parameters->AddItem(kHumanoidBossModelsGroup, "rightLegModelPath", std::string("Characters/right_leg.gltf"));
		// モデルパスのJSONキーは互換性維持のため英数字のまま、表示名だけ日本語にする。
		parameters->SetDisplayName(kHumanoidBossModelsGroup, "bodyModelPath", "胴体モデルパス");
		parameters->SetDisplayName(kHumanoidBossModelsGroup, "headModelPath", "頭モデルパス");
		parameters->SetDisplayName(kHumanoidBossModelsGroup, "leftArmModelPath", "左腕モデルパス");
		parameters->SetDisplayName(kHumanoidBossModelsGroup, "rightArmModelPath", "右腕モデルパス");
		parameters->SetDisplayName(kHumanoidBossModelsGroup, "leftLegModelPath", "左脚モデルパス");
		parameters->SetDisplayName(kHumanoidBossModelsGroup, "rightLegModelPath", "右脚モデルパス");
	}

	std::string GetModelPathOrDefault(const std::string& key, const std::string& defaultValue)
	{
		EnsureHumanoidModelParameters();
		try
		{
			return ParameterManager::GetInstance()->GetValue<std::string>(kHumanoidBossModelsGroup, key);
		}
		catch (const std::exception& e)
		{
			Log("[HumanoidBossBase] Failed to read HumanoidBossModels." + key + ": " + e.what() + ". Use default.\n");
			return defaultValue;
		}
	}
}


/// -------------------------------------------------------------
///							人型部位構築
/// -------------------------------------------------------------
void HumanoidBossBase::BuildBossParts()
{
	EnsureHumanoidModelParameters();

	BuildBodyHierarchy(
		{ GetBodyModelPath(), GetInitialBodyPosition(), {}, GetInitialBodyScale() },
		{
			{ GetHeadModelPath(), GetHeadLocalOffset(), {}, GetHeadScale() },
			{ GetLeftArmModelPath(), GetLeftArmLocalOffset(), {}, GetArmScale() },
			{ GetRightArmModelPath(), GetRightArmLocalOffset(), {}, GetArmScale() },
			{ GetLeftLegModelPath(), GetLeftLegLocalOffset(), {}, GetLegScale() },
			{ GetRightLegModelPath(), GetRightLegLocalOffset(), {}, GetLegScale() },
		});
}

std::string HumanoidBossBase::GetBodyModelPath() const
{
	return GetModelPathOrDefault("bodyModelPath", "Characters/body.gltf"); // 胴体モデルはJSONから参照し、読み込み失敗時は既定モデルへ戻す
}

std::string HumanoidBossBase::GetHeadModelPath() const
{
	return GetModelPathOrDefault("headModelPath", "Characters/head.gltf"); // 頭モデルはJSONから参照し、読み込み失敗時は既定モデルへ戻す
}

std::string HumanoidBossBase::GetLeftArmModelPath() const
{
	return GetModelPathOrDefault("leftArmModelPath", "Characters/left_arm.gltf"); // 左腕モデルはJSONから参照し、読み込み失敗時は既定モデルへ戻す
}

std::string HumanoidBossBase::GetRightArmModelPath() const
{
	return GetModelPathOrDefault("rightArmModelPath", "Characters/right_arm.gltf"); // 右腕モデルはJSONから参照し、読み込み失敗時は既定モデルへ戻す
}

std::string HumanoidBossBase::GetLeftLegModelPath() const
{
	return GetModelPathOrDefault("leftLegModelPath", "Characters/left_leg.gltf"); // 左脚モデルはJSONから参照し、読み込み失敗時は既定モデルへ戻す
}

std::string HumanoidBossBase::GetRightLegModelPath() const
{
	return GetModelPathOrDefault("rightLegModelPath", "Characters/right_leg.gltf"); // 右脚モデルはJSONから参照し、読み込み失敗時は既定モデルへ戻す
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
	const float desiredYaw = std::atan2(-toTarget.x, toTarget.z);
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
