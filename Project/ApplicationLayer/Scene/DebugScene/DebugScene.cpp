#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <LightManager.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#include <imgui.h>
#endif // USE_IMGUI

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// 衝突判定マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// DebugScene内だけでPhysicsWorldのRigidbody積分を確認する。
	physicsWorld_.RegisterRigidbody(&physicsTestRigidbody_);
	physicsStaticRigidbody_.SetBodyType(K4E::BodyType::Static);
	physicsStaticCollider_.SetRigidbody(&physicsStaticRigidbody_);
	physicsDynamicCollider_.SetRigidbody(&physicsTestRigidbody_);
	physicsWorld_.RegisterCollider(&physicsStaticCollider_);
	physicsWorld_.RegisterCollider(&physicsDynamicCollider_);
	ResetPhysicsDebugTest();
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	// 本編へ接続せず、DebugScene専用の物理テストだけを進める。
	UpdatePhysicsDebugTest(deltaTime);

	collisionManager_->CheckAllCollisions();
	collisionManager_->Update();
}

/// -------------------------------------------------------------
///							3D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw3DObjects()
{


#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });

	// PhysicsWorldテスト用の現在位置をワイヤースフィアで可視化する。
	Wireframe::GetInstance()->DrawSphere(physicsTestPosition_, 0.35f, { 0.2f, 0.9f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawAABB(physicsStaticCollider_.GetAABB(), { 1.0f, 0.8f, 0.15f, 1.0f });
	Wireframe::GetInstance()->DrawAABB(physicsDynamicCollider_.GetAABB(), { 0.2f, 0.9f, 1.0f, 1.0f });

	collisionManager_->Draw();
#endif // _DEBUG
}

/// -------------------------------------------------------------
///					シャドウマップ描画処理
/// -------------------------------------------------------------
void DebugScene::DrawShadowObjects()
{

}

/// -------------------------------------------------------------
///							2D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();


#pragma endregion
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void DebugScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	collisionManager_.reset();

	physicsWorld_.UnregisterRigidbody(&physicsTestRigidbody_);
	physicsWorld_.UnregisterCollider(&physicsStaticCollider_);
	physicsWorld_.UnregisterCollider(&physicsDynamicCollider_);

	input_ = nullptr;
	dxCommon_ = nullptr;
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	// ライトのImGui描画
	LightManager::GetInstance()->DrawImGui();

	// DebugScene専用のPhysicsWorld確認パネルを描画する。
	DrawPhysicsDebugImGui();

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///					 PhysicsWorld単体テストのリセット
/// -------------------------------------------------------------
void DebugScene::ResetPhysicsDebugTest()
{
	// 位置、速度、蓄積力を初期値へ戻して同じ条件で再確認できるようにする。
	physicsTestPosition_ = physicsTestInitialPosition_;
	physicsTestRigidbody_.SetBodyType(K4E::BodyType::Dynamic);
	physicsTestRigidbody_.SetMass(physicsTestMass_);
	physicsTestRigidbody_.SetUseGravity(physicsTestUseGravity_);
	physicsTestRigidbody_.SetVelocity(physicsTestInitialVelocity_);
	physicsTestRigidbody_.ClearForces();
	physicsWorld_.SetPositionSolveEnabled(physicsPositionSolveEnabled_);
	UpdatePhysicsDebugColliders();
}

/// -------------------------------------------------------------
///					 PhysicsWorld単体テストの更新
/// -------------------------------------------------------------
void DebugScene::UpdatePhysicsDebugTest(float deltaTime)
{
	// DebugScene側の位置をRigidbody速度で動かし、Colliderへ同期してからPhysicsWorldで接触を検出する。
	physicsTestRigidbody_.SetUseGravity(physicsTestUseGravity_);
	physicsTestRigidbody_.SetMass(physicsTestMass_);
	physicsTestPosition_ += physicsTestRigidbody_.GetVelocity() * deltaTime;
	UpdatePhysicsDebugColliders();
	physicsWorld_.SetPositionSolveEnabled(physicsPositionSolveEnabled_);
	physicsWorld_.Step(deltaTime);
	physicsTestPosition_ = physicsDynamicCollider_.GetCenterPosition();
}

/// -------------------------------------------------------------
///					 PhysicsWorld単体テストのImGui
/// -------------------------------------------------------------
void DebugScene::DrawPhysicsDebugImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin("PhysicsWorld Debug"))
	{
		const K4E::Vector3 velocity = physicsTestRigidbody_.GetVelocity();
		const std::vector<K4E::Contact>& contacts = physicsWorld_.GetContacts();
		const bool hasContact = !contacts.empty();

		// 物理テストの現在値を表示し、重力と質量はその場で調整できるようにする。
		ImGui::Text("Position: %.3f, %.3f, %.3f", physicsTestPosition_.x, physicsTestPosition_.y, physicsTestPosition_.z);
		ImGui::Text("Velocity: %.3f, %.3f, %.3f", velocity.x, velocity.y, velocity.z);
		if (ImGui::Checkbox("UseGravity", &physicsTestUseGravity_))
		{
			physicsTestRigidbody_.SetUseGravity(physicsTestUseGravity_);
		}
		if (ImGui::DragFloat("Mass", &physicsTestMass_, 0.05f, 0.1f, 100.0f))
		{
			physicsTestRigidbody_.SetMass(physicsTestMass_);
		}
		if (ImGui::DragFloat3("Dynamic Collider Position", &physicsTestPosition_.x, 0.05f))
		{
			UpdatePhysicsDebugColliders();
		}
		if (ImGui::DragFloat3("Static Collider Position", &physicsStaticColliderPosition_.x, 0.05f))
		{
			UpdatePhysicsDebugColliders();
		}
		if (ImGui::Checkbox("Resolve Enabled", &physicsPositionSolveEnabled_))
		{
			physicsWorld_.SetPositionSolveEnabled(physicsPositionSolveEnabled_);
		}

		// Contact生成結果をPhysicsWorldから直接読み、接触の有無と詳細値を確認する。
		ImGui::Separator();
		ImGui::Text("Contact Count: %zu", contacts.size());
		ImGui::Text("Contact: %s", hasContact ? "true" : "false");
		ImGui::Text("Dynamic Position: %.3f, %.3f, %.3f", physicsTestPosition_.x, physicsTestPosition_.y, physicsTestPosition_.z);
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
			ResetPhysicsDebugTest();
		}
	}
	ImGui::End();
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///					 PhysicsWorld単体テストColliderの更新
/// -------------------------------------------------------------
void DebugScene::UpdatePhysicsDebugColliders()
{
	// DebugScene専用AABBをColliderへ同期し、PhysicsWorld::DetectCollisions()が読める状態にする。
	physicsStaticCollider_.SetAABB({
		physicsStaticColliderPosition_ - physicsStaticColliderHalfSize_,
		physicsStaticColliderPosition_ + physicsStaticColliderHalfSize_,
		});
	physicsDynamicCollider_.SetAABB({
		physicsTestPosition_ - physicsDynamicColliderHalfSize_,
		physicsTestPosition_ + physicsDynamicColliderHalfSize_,
		});
}

/// -------------------------------------------------------------
///						Debug用の更新処理
/// -------------------------------------------------------------
void DebugScene::UpdateDebug()
{
	// Inputがない場合は何もしない（安全策）
	if (!input_) return;

	// F9はEditor操作中でも使いたいDebugショートカットなのでRaw入力で判定する
	if (input_->TriggerRawKey(DIK_F9))
	{
		// デバッグカメラの使用状態をトグルで切り替える
		const bool nextDebugCamera = !CameraManager::GetInstance()->IsUsingDebugCamera();

		// 切り替えた状態をCameraManagerとWireframeに伝える
		CameraManager::GetInstance()->SetUseDebugCamera(nextDebugCamera);
		Wireframe::GetInstance()->SetDebugCamera(nextDebugCamera);

		// DebugScene自身も状態を保持して、必要に応じて入力のロックやカーソルの表示を切り替える
		isDebugCamera_ = nextDebugCamera;

		// デバッグカメラ使用中はカーソルをロックして非表示にする。通常カメラ使用中はカーソルを表示してロック解除する。
		input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);
	}
}

