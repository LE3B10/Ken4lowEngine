#define NOMINMAX
#include "PhysicsDebugController.h"

#include "Wireframe.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

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
	physicsWorld_.SetPositionSolveEnabled(enableResolve_);

	if (!enablePhysicsStep_)
	{
		UpdateTestColliders();
		return;
	}

	// Rigidbodyの速度でテスト位置を進め、Collider同期後にPhysicsWorldで接触解決する。
	dynamicPosition_ += dynamicRigidbody_.GetVelocity() * deltaTime;
	UpdateTestColliders();
	physicsWorld_.Step(deltaTime);
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
			ImGui::Text("Dynamic Position: %.3f, %.3f, %.3f", dynamicPosition_.x, dynamicPosition_.y, dynamicPosition_.z);
			ImGui::Text("Dynamic Velocity: %.3f, %.3f, %.3f", velocity.x, velocity.y, velocity.z);
			ImGui::Text("IsGrounded: %s", dynamicRigidbody_.IsGrounded() ? "true" : "false");
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
			if (ImGui::DragFloat3("Dynamic Position", &dynamicPosition_.x, 0.05f))
			{
				UpdateTestColliders();
			}
			if (ImGui::DragFloat3("Static Position", &staticPosition_.x, 0.05f))
			{
				UpdateTestColliders();
			}

			// Contact生成結果をPhysicsWorldから直接読み、接触の有無と詳細値を確認する。
			ImGui::Separator();
			ImGui::Text("Contact Count: %zu", contacts.size());
			ImGui::Text("Contact: %s", hasContact ? "true" : "false");
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
	dynamicRigidbody_.SetBodyType(K4E::BodyType::Dynamic);
	dynamicRigidbody_.SetMass(mass_);
	dynamicRigidbody_.SetUseGravity(useGravity_);
	dynamicRigidbody_.SetRestitution(restitution_);
	dynamicRigidbody_.SetVelocity(dynamicInitialVelocity_);
	dynamicRigidbody_.ClearForces();
	dynamicRigidbody_.ClearFrameState();
	physicsWorld_.SetPositionSolveEnabled(enableResolve_);
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
