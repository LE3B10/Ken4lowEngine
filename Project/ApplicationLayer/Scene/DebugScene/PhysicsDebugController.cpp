#define NOMINMAX
#include "PhysicsDebugController.h"

#include "Wireframe.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace
{
	const char* ToResponseName(K4E::CollisionResponseType response)
	{
		switch (response)
		{
		case K4E::CollisionResponseType::Ignore:
			return "Ignore";
		case K4E::CollisionResponseType::Trigger:
			return "Trigger";
		case K4E::CollisionResponseType::Block:
			return "Block";
		default:
			return "Unknown";
		}
	}

	K4E::CollisionResponseType ToResponseType(int responseTypeIndex)
	{
		switch (responseTypeIndex)
		{
		case 0:
			return K4E::CollisionResponseType::Ignore;
		case 1:
			return K4E::CollisionResponseType::Trigger;
		case 2:
		default:
			return K4E::CollisionResponseType::Block;
		}
	}
}

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void PhysicsDebugController::Initialize()
{
	// DebugScene内だけで使うRigidbodyとColliderをPhysicsWorldへ登録する。
	physicsWorld_.RegisterRigidbody(&dynamicRigidbody_);
	staticRigidbody_.SetBodyType(K4E::BodyType::Static);
	staticCollider_.SetRigidbody(&staticRigidbody_);
	dynamicCollider_.SetRigidbody(&dynamicRigidbody_);
	physicsWorld_.RegisterCollider(&staticCollider_);
	physicsWorld_.RegisterCollider(&dynamicCollider_);
	ApplyResponseSetting();
	ResetTestObjects();
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void PhysicsDebugController::Update(float deltaTime)
{
	// DebugScene専用の物理確認処理を更新する。本編へ接続せず、このController内だけでStepする。
	dynamicRigidbody_.SetUseGravity(useGravity_);
	dynamicRigidbody_.SetMass(mass_);
	dynamicRigidbody_.SetRestitution(restitution_);
	dynamicRigidbody_.SetStaticFriction(staticFriction_);
	dynamicRigidbody_.SetDynamicFriction(dynamicFriction_);
	dynamicRigidbody_.SetSleepEnabled(enableSleep_);
	dynamicRigidbody_.SetSleepSpeedThreshold(sleepSpeedThreshold_);
	dynamicRigidbody_.SetSleepTimeThreshold(sleepTimeThreshold_);
	physicsWorld_.SetPositionSolveEnabled(enableResolve_);
	physicsWorld_.SetFrictionSolveEnabled(enableFriction_);

	if (!enablePhysicsStep_)
	{
		UpdateTestColliders();
		return;
	}

	// Rigidbodyの速度でDebug用テスト位置を進め、物理処理自体はPhysicsWorld::Update()の固定更新経由で確認する。
	dynamicPosition_ += dynamicRigidbody_.GetVelocity() * deltaTime;
	UpdateTestColliders();
	physicsWorld_.Update(deltaTime);
	dynamicPosition_ = dynamicCollider_.GetCenterPosition();
}

/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void PhysicsDebugController::Draw()
{
	// Dynamic/Staticの確認形状をワイヤー表示し、Contact normalも視覚化する。
	K4E::Wireframe* wireframe = K4E::Wireframe::GetInstance();
	wireframe->DrawSphere(dynamicPosition_, 0.35f, { 0.2f, 0.9f, 1.0f, 1.0f });
	wireframe->DrawAABB(staticCollider_.GetAABB(), { 1.0f, 0.8f, 0.15f, 1.0f });
	wireframe->DrawAABB(dynamicCollider_.GetAABB(), { 0.2f, 0.9f, 1.0f, 1.0f });

	const std::vector<K4E::Contact>& contacts = physicsWorld_.GetContacts();
	if (!contacts.empty())
	{
		const K4E::Contact& contact = contacts.front();
		wireframe->DrawLine(contact.point, contact.point + contact.normal * 1.5f, { 1.0f, 0.2f, 0.2f, 1.0f });
	}
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void PhysicsDebugController::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin("PhysicsWorld Debug"))
	{
		if (ImGui::CollapsingHeader("Physics Debug Controller", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const K4E::Vector3 velocity = dynamicRigidbody_.GetVelocity();
			const std::vector<K4E::Contact>& contacts = physicsWorld_.GetContacts();
			const bool hasContact = !contacts.empty();

			// 物理テストの現在値とStep/Resolve状態をDebugScene上で調整できるようにする。
			ImGui::Checkbox("Enable Physics Step", &enablePhysicsStep_);
			if (ImGui::Checkbox("Enable Resolve", &enableResolve_))
			{
				physicsWorld_.SetPositionSolveEnabled(enableResolve_);
			}
			if (ImGui::Checkbox("Enable Friction", &enableFriction_))
			{
				physicsWorld_.SetFrictionSolveEnabled(enableFriction_);
			}
			if (ImGui::Checkbox("Enable Sleep", &enableSleep_))
			{
				dynamicRigidbody_.SetSleepEnabled(enableSleep_);
			}

			// PhysicsWorldの固定更新設定をDebugScene上で切り替え、サブステップの動きを確認できるようにする。
			bool useFixedStep = physicsWorld_.IsUseFixedStep();
			if (ImGui::Checkbox("Use Fixed Step", &useFixedStep))
			{
				physicsWorld_.SetUseFixedStep(useFixedStep);
			}
			float fixedTimeStep = physicsWorld_.GetFixedTimeStep();
			if (ImGui::DragFloat("Fixed Time Step", &fixedTimeStep, 0.001f, 1.0f / 240.0f, 1.0f / 15.0f, "%.4f"))
			{
				physicsWorld_.SetFixedTimeStep(fixedTimeStep);
			}
			float maxDeltaTime = physicsWorld_.GetMaxDeltaTime();
			if (ImGui::DragFloat("Max Delta Time", &maxDeltaTime, 0.001f, 0.016f, 0.5f, "%.4f"))
			{
				physicsWorld_.SetMaxDeltaTime(maxDeltaTime);
			}
			int maxSubSteps = physicsWorld_.GetMaxSubSteps();
			if (ImGui::DragInt("Max Sub Steps", &maxSubSteps, 1.0f, 1, 16))
			{
				physicsWorld_.SetMaxSubSteps(maxSubSteps);
			}
			ImGui::Text("Accumulator: %.4f", physicsWorld_.GetAccumulator());
			ImGui::Text("Last Sub Step Count: %d", physicsWorld_.GetLastSubStepCount());

			ImGui::Text("Dynamic Position: %.3f, %.3f, %.3f", dynamicPosition_.x, dynamicPosition_.y, dynamicPosition_.z);
			ImGui::Text("Dynamic Velocity: %.3f, %.3f, %.3f", velocity.x, velocity.y, velocity.z);
			ImGui::Text("IsGrounded: %s", dynamicRigidbody_.IsGrounded() ? "true" : "false");
			ImGui::Text("Is Sleeping: %s", dynamicRigidbody_.IsSleeping() ? "true" : "false");
			ImGui::Text("Sleep Timer: %.3f", dynamicRigidbody_.GetSleepTimer());
			if (ImGui::Checkbox("UseGravity", &useGravity_))
			{
				dynamicRigidbody_.SetUseGravity(useGravity_);
			}
			if (ImGui::DragFloat("Mass", &mass_, 0.05f, 0.1f, 100.0f))
			{
				dynamicRigidbody_.SetMass(mass_);
			}
			if (ImGui::DragFloat("Restitution", &restitution_, 0.01f, 0.0f, 1.0f))
			{
				dynamicRigidbody_.SetRestitution(restitution_);
				restitution_ = dynamicRigidbody_.GetRestitution();
			}
			if (ImGui::DragFloat("Static Friction", &staticFriction_, 0.01f, 0.0f, 10.0f))
			{
				dynamicRigidbody_.SetStaticFriction(staticFriction_);
				staticFriction_ = dynamicRigidbody_.GetStaticFriction();
			}
			if (ImGui::DragFloat("Dynamic Friction", &dynamicFriction_, 0.01f, 0.0f, 10.0f))
			{
				dynamicRigidbody_.SetDynamicFriction(dynamicFriction_);
				dynamicFriction_ = dynamicRigidbody_.GetDynamicFriction();
			}
			if (ImGui::DragFloat("Sleep Speed Threshold", &sleepSpeedThreshold_, 0.01f, 0.0f, 10.0f))
			{
				dynamicRigidbody_.SetSleepSpeedThreshold(sleepSpeedThreshold_);
				sleepSpeedThreshold_ = dynamicRigidbody_.GetSleepSpeedThreshold();
			}
			if (ImGui::DragFloat("Sleep Time Threshold", &sleepTimeThreshold_, 0.01f, 0.0f, 10.0f))
			{
				dynamicRigidbody_.SetSleepTimeThreshold(sleepTimeThreshold_);
				sleepTimeThreshold_ = dynamicRigidbody_.GetSleepTimeThreshold();
			}
			if (ImGui::DragFloat("Initial Horizontal Speed", &initialHorizontalSpeed_, 0.05f, -20.0f, 20.0f))
			{
				dynamicInitialVelocity_.x = initialHorizontalSpeed_;
			}
			if (ImGui::DragFloat3("Dynamic Position", &dynamicPosition_.x, 0.05f))
			{
				dynamicRigidbody_.WakeUp();
				UpdateTestColliders();
			}
			if (ImGui::DragFloat3("Static Position", &staticPosition_.x, 0.05f))
			{
				UpdateTestColliders();
			}

			// DebugScene上でResponse挙動を確認するため、Layerペアと応答種別を切り替えられるようにする。
			ImGui::Separator();
			if (ImGui::DragInt("Dynamic Collider Layer", &dynamicLayer_, 1.0f, 0, static_cast<int>(K4E::CollisionResponseMatrix::kMaxCollisionLayers) - 1))
			{
				dynamicLayer_ = std::clamp(dynamicLayer_, 0, static_cast<int>(K4E::CollisionResponseMatrix::kMaxCollisionLayers) - 1);
				dynamicCollider_.SetCollisionLayer(static_cast<uint32_t>(dynamicLayer_));
			}
			if (ImGui::DragInt("Static Collider Layer", &staticLayer_, 1.0f, 0, static_cast<int>(K4E::CollisionResponseMatrix::kMaxCollisionLayers) - 1))
			{
				staticLayer_ = std::clamp(staticLayer_, 0, static_cast<int>(K4E::CollisionResponseMatrix::kMaxCollisionLayers) - 1);
				staticCollider_.SetCollisionLayer(static_cast<uint32_t>(staticLayer_));
			}
			const char* responseItems[] = { "Ignore", "Trigger", "Block" };
			ImGui::Combo("Response Type", &responseTypeIndex_, responseItems, 3);
			if (ImGui::Button("Apply Response"))
			{
				ApplyResponseSetting();
			}
			const K4E::CollisionResponseType currentResponse = physicsWorld_.GetResponseMatrix().GetResponse(
				static_cast<uint32_t>(dynamicLayer_),
				static_cast<uint32_t>(staticLayer_));
			ImGui::Text("Current Response: %s", ToResponseName(currentResponse));

			// Contact生成結果をPhysicsWorldから直接読み、接触の有無と詳細値を確認する。
			ImGui::Separator();
			ImGui::Text("Contact Count: %zu", contacts.size());
			ImGui::Text("Contact: %s", hasContact ? "true" : "false");
			ImGui::Text("Is Trigger Contact: %s", (hasContact && contacts.front().isTrigger) ? "true" : "false");
			if (hasContact)
			{
				const K4E::Contact& contact = contacts.front();
				ImGui::Text("Contact normal: %.3f, %.3f, %.3f", contact.normal.x, contact.normal.y, contact.normal.z);
				ImGui::Text("Contact penetration: %.3f", contact.penetration);
			}
			else
			{
				ImGui::Text("Contact normal: 0.000, 0.000, 0.000");
				ImGui::Text("Contact penetration: 0.000");
			}
			if (ImGui::Button("Reset"))
			{
				ResetTestObjects();
			}
			ImGui::SameLine();
			if (ImGui::Button("Wake Up"))
			{
				dynamicRigidbody_.WakeUp();
			}
		}
	}
	ImGui::End();
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///						テストオブジェクトリセット
/// -------------------------------------------------------------
void PhysicsDebugController::ResetTestObjects()
{
	// DynamicとStaticの位置、速度、蓄積力を初期値へ戻し、同じ条件で再確認できるようにする。
	dynamicPosition_ = dynamicInitialPosition_;
	staticPosition_ = staticInitialPosition_;
	dynamicInitialVelocity_.x = initialHorizontalSpeed_;
	dynamicRigidbody_.SetBodyType(K4E::BodyType::Dynamic);
	dynamicRigidbody_.SetMass(mass_);
	dynamicRigidbody_.SetUseGravity(useGravity_);
	dynamicRigidbody_.SetRestitution(restitution_);
	dynamicRigidbody_.SetStaticFriction(staticFriction_);
	dynamicRigidbody_.SetDynamicFriction(dynamicFriction_);
	dynamicRigidbody_.SetSleepEnabled(enableSleep_);
	dynamicRigidbody_.SetSleepSpeedThreshold(sleepSpeedThreshold_);
	dynamicRigidbody_.SetSleepTimeThreshold(sleepTimeThreshold_);
	dynamicRigidbody_.SetVelocity(dynamicInitialVelocity_);
	dynamicRigidbody_.ClearForces();
	dynamicRigidbody_.ClearFrameState();
	physicsWorld_.SetPositionSolveEnabled(enableResolve_);
	physicsWorld_.SetFrictionSolveEnabled(enableFriction_);
	ApplyResponseSetting();
	UpdateTestColliders();
}

/// -------------------------------------------------------------
///						Collider同期処理
/// -------------------------------------------------------------
void PhysicsDebugController::UpdateTestColliders()
{
	// DebugScene専用AABBをColliderへ同期し、PhysicsWorld::DetectCollisions()が読める状態にする。
	staticCollider_.SetAABB({
		staticPosition_ - staticHalfSize_,
		staticPosition_ + staticHalfSize_,
		});
	dynamicCollider_.SetAABB({
		dynamicPosition_ - dynamicHalfSize_,
		dynamicPosition_ + dynamicHalfSize_,
		});
}

/// -------------------------------------------------------------
///						Response設定適用
/// -------------------------------------------------------------
void PhysicsDebugController::ApplyResponseSetting()
{
	// DebugScene上でResponse挙動を確認するため、Collider LayerとMatrix設定を同期する。
	dynamicLayer_ = std::clamp(dynamicLayer_, 0, static_cast<int>(K4E::CollisionResponseMatrix::kMaxCollisionLayers) - 1);
	staticLayer_ = std::clamp(staticLayer_, 0, static_cast<int>(K4E::CollisionResponseMatrix::kMaxCollisionLayers) - 1);
	responseTypeIndex_ = std::clamp(responseTypeIndex_, 0, 2);
	dynamicCollider_.SetCollisionLayer(static_cast<uint32_t>(dynamicLayer_));
	staticCollider_.SetCollisionLayer(static_cast<uint32_t>(staticLayer_));
	physicsWorld_.GetResponseMatrix().SetResponse(
		static_cast<uint32_t>(dynamicLayer_),
		static_cast<uint32_t>(staticLayer_),
		ToResponseType(responseTypeIndex_));
}
