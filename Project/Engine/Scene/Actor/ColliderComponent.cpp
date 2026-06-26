#include "ColliderComponent.h"
#include <Actor.h>
#include <SceneComponent.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void ColliderComponent::Initialize()
	{
		SceneComponent::Initialize(); // 親子関係を考慮したWorldTransformを初期計算する

		collider_ = std::make_unique<Collider>();	   // Colliderを生成する
		collider_->Initialize();					   // Colliderの初期化処理を行う
		collider_->SetDebugName(GetName());			   // Colliderのデバッグ名を設定する
		collider_->SetCollisionLayer(collisionLayer_); // Colliderの衝突レイヤーを設定する
		collider_->SetOwner(this);					   // Colliderの所有者を設定する

		SyncFromSceneTransform();
	}

	void ColliderComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime); // SceneComponent側のTransformを更新する。
		SyncFromSceneTransform();
	}

	void ColliderComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!collider_)
		{
			return; // Colliderが生成されていない場合は同期しない
		}

		Actor* owner = GetOwner();
		if (!owner)
		{
			return; // 所有者Actorが存在しない場合は同期しない
		}

		SceneComponent* root = owner->GetRootComponent();
		if (!root)
		{
			return; // 所有者ActorのRootComponentが存在しない場合は同期しない
		}

		const Vector3 currentWorldPosition = GetWorldPosition();
		const Vector3 correctedWorldPosition = collider_->GetCenterPosition();
		const Vector3 correctionDelta = correctedWorldPosition - currentWorldPosition;

		constexpr float kCorrectionEpsilon = 0.0001f; // 許容誤差の閾値
		if (Vector3::Length(correctionDelta) <= kCorrectionEpsilon)
		{
			return; // 補正差分がほぼない場合はTransformを動かさない
		}

		if (root == this)
		{
			LocalPosition() += correctionDelta; // RootComponentの場合はLocalPositionを直接修正する
		}
		else
		{
			root->LocalPosition() += correctionDelta; // RootComponent以外の場合はRootComponentのLocalPositionを修正する
		}

		root->RefreshWorldTransform(); // RootComponentのWorldTransformを更新する
		SyncFromSceneTransform();	   // SceneComponentのTransformをColliderへ再同期する
	}

	void ColliderComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // ColliderのTransform情報を確認・編集できるようにする。

#ifdef USE_IMGUI

		const char* shapeNames[] =
		{
			"None",
			"Sphere",
			"AABB",
			"OBB",
			"Capsule",
			"Segment",
		};

		int shapeIndex = static_cast<int>(shapeType_);

		ImGui::SeparatorText("Collider Component");
		if (ImGui::Combo("Shape Type", &shapeIndex, shapeNames, IM_ARRAYSIZE(shapeNames)))
		{
			shapeType_ = static_cast<ECollisionShapeType>(shapeIndex);
		}

		if (shapeType_ == ECollisionShapeType::Sphere)
		{
			ImGui::DragFloat("Radius", &radius_, 0.1f, 0.0f, 100.0f);
		}
		else if (shapeType_ == ECollisionShapeType::AABB || shapeType_ == ECollisionShapeType::OBB)
		{
			ImGui::DragFloat3("Half Size", &halfSize_.x, 0.1f, 0.0f, 100.0f);
		}

#endif
	}

	void ColliderComponent::Finalize()
	{
		collider_.reset(); // Colliderを破棄する
	}

	void ColliderComponent::SetCollisionLayer(uint32_t layer)
	{
		collisionLayer_ = layer; // ColliderComponentの衝突レイヤーを設定する

		if (collider_)
		{
			collider_->SetCollisionLayer(collisionLayer_); // Colliderの衝突レイヤーを設定する
		}
	}

	void ColliderComponent::SetCollisionLayer(PhysicsCollisionLayer layer)
	{
		// 名前付きレイヤーを既存の数値レイヤーへ変換して設定する
		SetCollisionLayer(static_cast<uint32_t>(layer));
	}

	void ColliderComponent::SyncFromSceneTransform()
	{
		if (!collider_)
		{
			return; // Colliderが生成されていない場合は同期しない
		}

		switch (shapeType_)
		{
		case Ken4lowEngine::ECollisionShapeType::None:
			break;
		case Ken4lowEngine::ECollisionShapeType::Sphere:
			{
				Sphere sphere{};
				sphere.center = GetWorldPosition(); // SceneComponentのWorld位置をColliderの中心に反映する
				sphere.radius = radius_;			// 設定された半径をColliderに反映する
				collider_->SetSphere(sphere);		// ColliderにSphere情報を設定する
				break;
			}
			break;
		case Ken4lowEngine::ECollisionShapeType::AABB:
			{
				Vector3 center = GetWorldPosition(); // SceneComponentのWorld位置をColliderの中心に反映する

				AABB aabb{};
				aabb.min = center - halfSize_;
				aabb.max = center + halfSize_;
				collider_->SetAABB(aabb);
			}
			break;
		case Ken4lowEngine::ECollisionShapeType::OBB:
			{
				collider_->SetCenterPosition(GetWorldPosition()); // SceneComponentのWorld位置をColliderの中心に反映する
				collider_->SetOBBHalfSize(halfSize_);
				collider_->SetOrientation(GetWorldRotation()); // SceneComponentのWorld回転をColliderの回転に反映する
				break;
			}
			break;
		case Ken4lowEngine::ECollisionShapeType::Capsule:
			break;
		case Ken4lowEngine::ECollisionShapeType::Segment:
			break;
		default:
			break;
		}
	}
}