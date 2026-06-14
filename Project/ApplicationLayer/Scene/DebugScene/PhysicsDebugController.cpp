#define NOMINMAX
#include "PhysicsDebugController.h"

#include "Wireframe.h"

#include <algorithm>
#include <cstdint>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace
{
	constexpr size_t kMaxEventLogCount = 20u;

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

	const char* ToPhysicsEventName(K4E::PhysicsEventType eventType)
	{
		switch (eventType)
		{
		case K4E::PhysicsEventType::CollisionEnter:
			return "CollisionEnter";
		case K4E::PhysicsEventType::CollisionStay:
			return "CollisionStay";
		case K4E::PhysicsEventType::CollisionExit:
			return "CollisionExit";
		case K4E::PhysicsEventType::TriggerEnter:
			return "TriggerEnter";
		case K4E::PhysicsEventType::TriggerStay:
			return "TriggerStay";
		case K4E::PhysicsEventType::TriggerExit:
			return "TriggerExit";
		default:
			return "Unknown";
		}
	}
}

/// -------------------------------------------------------------
///							破棄処理
/// -------------------------------------------------------------
PhysicsDebugController::~PhysicsDebugController()
{
	// 破棄済みポインタ参照を防ぐため、PhysicsWorldからDebug用リスナー登録を解除する。
	physicsParameterBridge_.Finalize(this);
	stagePhysicsBinder_.Unbind();
	physicsWorld_.RemovePhysicsEventListener(this);
}

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void PhysicsDebugController::Initialize()
{
	// DebugScene内だけで使うRigidbodyとColliderをPhysicsWorldへ登録する。
	physicsParameterBridge_.Initialize();
	physicsParameterBridge_.RegisterAppliers(this, [this]() { ApplyParameterSettings(); });
	physicsWorld_.RegisterRigidbody(&dynamicRigidbody_);
	// DebugScene専用の物理イベント確認のため、PhysicsDebugController自身をリスナー登録する。
	physicsWorld_.AddPhysicsEventListener(this);
	staticRigidbody_.SetBodyType(K4E::BodyType::Static);
	staticCollider_.SetRigidbody(&staticRigidbody_);
	dynamicCollider_.SetRigidbody(&dynamicRigidbody_);
	physicsWorld_.RegisterCollider(&staticCollider_);
	physicsWorld_.RegisterCollider(&dynamicCollider_);
	InitializeDebugStageColliders();
	ApplyResponseSetting();
	ResetTestObjects();
	ApplyParameterSettings();
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
	physicsWorld_.SetVelocitySolveEnabled(enableVelocityResolve_);
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

	K4E::Vector4 dynamicColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	if (isCollisionTouching_)
	{
		dynamicColor = { 1.0f, 0.15f, 0.1f, 1.0f };
	}
	else if (isTriggerTouching_)
	{
		dynamicColor = { 1.0f, 0.9f, 0.1f, 1.0f };
	}

	wireframe->DrawSphere(dynamicPosition_, 0.35f, dynamicColor);
	wireframe->DrawAABB(staticCollider_.GetAABB(), { 1.0f, 0.8f, 0.15f, 1.0f });
	wireframe->DrawAABB(dynamicCollider_.GetAABB(), dynamicColor);

	if (showStagePhysicsColliders_ && stagePhysicsBinder_.IsBound())
	{
		// PhysicsWorldへ登録済みの仮Stage Colliderを表示し、Binder経由の登録状態を視覚確認する。
		for (const K4E::Collider& collider : debugStageColliders_)
		{
			wireframe->DrawAABB(collider.GetAABB(), { 0.45f, 1.0f, 0.35f, 1.0f });
		}
	}

	const std::vector<K4E::Contact>& contacts = physicsWorld_.GetContacts();
	if (!contacts.empty())
	{
		const K4E::Contact& contact = contacts.front();
		wireframe->DrawLine(contact.point, contact.point + contact.normal * 1.5f, { 1.0f, 0.2f, 0.2f, 1.0f });
	}

	// 共通Debug描画を通して、Collider/Contact/Rigidbody/Eventの状態をDebugSceneでも確認する。
	physicsDebugDraw_.Draw(physicsWorld_);
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
			if (ImGui::Checkbox("Enable Velocity Resolve", &enableVelocityResolve_))
			{
				physicsWorld_.SetVelocitySolveEnabled(enableVelocityResolve_);
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

		if (ImGui::CollapsingHeader("Physics Events", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const std::vector<K4E::PhysicsEvent>& events = physicsWorld_.GetEvents();

			// DebugScene上でPhysicsEventの発生を確認するため、直近Stepのイベントログを表示する。
			ImGui::Text("Event Count: %zu", events.size());
			const size_t displayCount = std::min<size_t>(events.size(), 12u);
			for (size_t i = 0; i < displayCount; ++i)
			{
				const K4E::PhysicsEvent& event = events[events.size() - 1u - i];
				ImGui::Separator();
				ImGui::Text("Event Type: %s", ToPhysicsEventName(event.type));
				ImGui::Text("ColliderA: %p", static_cast<void*>(event.colliderA));
				ImGui::Text("ColliderB: %p", static_cast<void*>(event.colliderB));
				ImGui::Text("isTrigger: %s", event.isTrigger ? "true" : "false");
			}
		}

		if (ImGui::CollapsingHeader("Physics Event Reaction", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// PhysicsWorldのリスナー通知に反応したDebugScene専用状態を表示する。
			ImGui::Text("Trigger Touching: %s", isTriggerTouching_ ? "true" : "false");
			ImGui::Text("Collision Touching: %s", isCollisionTouching_ ? "true" : "false");
			ImGui::Text("Trigger Enter / Stay / Exit Count: %d / %d / %d", triggerEnterCount_, triggerStayCount_, triggerExitCount_);
			ImGui::Text("Collision Enter / Stay / Exit Count: %d / %d / %d", collisionEnterCount_, collisionStayCount_, collisionExitCount_);
			if (ImGui::Button("Clear Event Logs"))
			{
				ClearEventReactionState();
			}

			ImGui::Text("Latest Event Logs");
			for (const std::string& log : eventLogs_)
			{
				ImGui::BulletText("%s", log.c_str());
			}
		}

		if (ImGui::CollapsingHeader("Stage Physics Binder", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// DebugScene上でStage Collider群のPhysicsWorld登録・解除を確認する。
			if (ImGui::Button("Bind Stage Colliders"))
			{
				stagePhysicsBinder_.Bind(physicsWorld_, debugStageColliderPointers_);
			}
			ImGui::SameLine();
			if (ImGui::Button("Unbind Stage Colliders"))
			{
				stagePhysicsBinder_.Unbind();
			}
			ImGui::Checkbox("Show Stage Physics Colliders", &showStagePhysicsColliders_);
			ImGui::Text("Is Bound: %s", stagePhysicsBinder_.IsBound() ? "true" : "false");
			ImGui::Text("Bound Collider Count: %zu", stagePhysicsBinder_.GetBoundColliderCount());
			ImGui::Text("PhysicsWorld Collider Count: %zu", physicsWorld_.GetColliderCount());
		}

		physicsDebugDraw_.DrawImGui(physicsWorld_);
		physicsParameterBridge_.DrawImGui();
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
	physicsWorld_.SetVelocitySolveEnabled(enableVelocityResolve_);
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

/// -------------------------------------------------------------
///						仮Stage Collider初期化
/// -------------------------------------------------------------
void PhysicsDebugController::InitializeDebugStageColliders()
{
	// StagePhysicsBinderの登録確認用に、Rigidbodyを持たないStatic扱いの仮Stage Colliderを用意する。
	debugStageColliders_.clear();
	debugStageColliderPointers_.clear();
	debugStageColliders_.resize(3);

	const K4E::Vector3 centers[] = {
		{ 8.0f, -0.25f, 0.0f },
		{ 11.0f, 0.75f, 0.0f },
		{ 14.0f, 0.25f, 0.0f },
	};
	const K4E::Vector3 halfSizes[] = {
		{ 1.0f, 0.25f, 1.0f },
		{ 0.5f, 1.25f, 0.5f },
		{ 1.25f, 0.5f, 1.25f },
	};

	for (size_t i = 0; i < debugStageColliders_.size(); ++i)
	{
		K4E::Collider& collider = debugStageColliders_[i];
		collider.SetRigidbody(nullptr);
		collider.SetCollisionLayer(0u);
		collider.SetAABB({
			centers[i] - halfSizes[i],
			centers[i] + halfSizes[i],
			});
		debugStageColliderPointers_.push_back(&collider);
	}
}

/// -------------------------------------------------------------
///						物理イベント通知処理
/// -------------------------------------------------------------
void PhysicsDebugController::OnPhysicsEvent(const K4E::PhysicsEvent& event)
{
	// PhysicsWorldから届いたイベントをDebugScene上の確認用状態へ反映する。
	switch (event.type)
	{
	case K4E::PhysicsEventType::TriggerEnter:
		isTriggerTouching_ = true;
		++triggerEnterCount_;
		break;
	case K4E::PhysicsEventType::TriggerStay:
		++triggerStayCount_;
		break;
	case K4E::PhysicsEventType::TriggerExit:
		isTriggerTouching_ = false;
		++triggerExitCount_;
		break;
	case K4E::PhysicsEventType::CollisionEnter:
		isCollisionTouching_ = true;
		++collisionEnterCount_;
		break;
	case K4E::PhysicsEventType::CollisionStay:
		++collisionStayCount_;
		break;
	case K4E::PhysicsEventType::CollisionExit:
		isCollisionTouching_ = false;
		++collisionExitCount_;
		break;
	default:
		break;
	}

	AddEventLog(event);
}

/// -------------------------------------------------------------
///						イベント反応状態クリア
/// -------------------------------------------------------------
void PhysicsDebugController::ClearEventReactionState()
{
	// Debug確認をやり直せるよう、イベント由来の表示状態とカウントをまとめて初期化する。
	isTriggerTouching_ = false;
	isCollisionTouching_ = false;
	triggerEnterCount_ = 0;
	triggerStayCount_ = 0;
	triggerExitCount_ = 0;
	collisionEnterCount_ = 0;
	collisionStayCount_ = 0;
	collisionExitCount_ = 0;
	eventLogs_.clear();
}

/// -------------------------------------------------------------
///						イベントログ追加
/// -------------------------------------------------------------
void PhysicsDebugController::AddEventLog(const K4E::PhysicsEvent& event)
{
	// 最新イベントを先頭へ積み、Debug表示用ログが増え続けないよう最大件数で切る。
	std::string log = ToPhysicsEventName(event.type);
	log += event.isTrigger ? " | Trigger" : " | Collision";
	log += " | A:";
	log += event.colliderA == &dynamicCollider_ ? "Dynamic" : event.colliderA == &staticCollider_ ? "Static" : "Stage/Unknown";
	log += " B:";
	log += event.colliderB == &dynamicCollider_ ? "Dynamic" : event.colliderB == &staticCollider_ ? "Static" : "Stage/Unknown";

	eventLogs_.insert(eventLogs_.begin(), log);
	if (eventLogs_.size() > kMaxEventLogCount)
	{
		eventLogs_.resize(kMaxEventLogCount);
	}
}

/// -------------------------------------------------------------
///						ParameterManager反映
/// -------------------------------------------------------------
void PhysicsDebugController::ApplyParameterSettings()
{
	// JSON/ImGuiで調整した値をDebugScene専用のPhysicsWorldとDebugDrawへ反映する。
	physicsParameterBridge_.ApplyTo(physicsWorld_);
	physicsParameterBridge_.ApplyTo(physicsDebugDraw_);
	enableResolve_ = physicsWorld_.IsPositionSolveEnabled();
	enableVelocityResolve_ = physicsWorld_.IsVelocitySolveEnabled();
	enableFriction_ = physicsWorld_.IsFrictionSolveEnabled();
	enableSleep_ = physicsParameterBridge_.GetWorldSettings().enableSleep;
}
