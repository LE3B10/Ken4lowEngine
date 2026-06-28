#include "ColliderComponent.h"
#include <Actor.h>
#include <SceneComponent.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// Collision形状をJSON保存用文字列へ変換する。
		/// </summary>
		const char* Tostring(ECollisionShapeType shapeType)
		{
			switch (shapeType)
			{
			case Ken4lowEngine::ECollisionShapeType::Sphere:
				return "Sphere";
			case Ken4lowEngine::ECollisionShapeType::AABB:
				return "AABB";
			case Ken4lowEngine::ECollisionShapeType::OBB:
				return "OBB";
			case Ken4lowEngine::ECollisionShapeType::Capsule:
				return "Capsule";
			case Ken4lowEngine::ECollisionShapeType::Segment:
				return "Segment";
			case Ken4lowEngine::ECollisionShapeType::None:
			default:
				return "None";
			}
		}

		/// <summary>
		/// JSON文字列からCollision形状を変換する
		/// </summary>
		ECollisionShapeType ShapeTypeFromString(const std::string& shapeType)
		{
			if (shapeType == "Sphere")
			{
				return ECollisionShapeType::Sphere;
			}
			else if (shapeType == "AABB")
			{
				return ECollisionShapeType::AABB;
			}
			else if (shapeType == "OBB")
			{
				return ECollisionShapeType::OBB;
			}
			else if (shapeType == "Capsule")
			{
				return ECollisionShapeType::Capsule;
			}
			else if (shapeType == "Segment")
			{
				return ECollisionShapeType::Segment;
			}

			return ECollisionShapeType::None;
		}

		Vector3 ReadVector3FromJson(const nlohmann::json& json, const std::string& key, const Vector3& defaultValue = Vector3{ 0.0f, 0.0f, 0.0f })
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3)
			{
				return defaultValue; // 値が存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>()
			};
		}
	}

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

	void ColliderComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName();								// ColliderComponentのクラス種別をJSONへ保存する
		outJson["ShapeType"] = Tostring(shapeType_);						// Colliderの形状種別をJSONへ保存する
		outJson["HalfSize"] = { halfSize_.x, halfSize_.y, halfSize_.z };	// AABB / OBBの半サイズをJSONへ保存する
		outJson["IsTrigger"] = isTrigger_;									// Trigger判定かどうかをJSONへ保存する
		outJson["CollisionLayer"] = static_cast<uint32_t>(collisionLayer_); // Colliderの衝突レイヤーをJSONへ保存する
	}

	void ColliderComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		if (inJson.contains("ShapeType") && inJson["ShapeType"].is_string())
		{
			SetShapeType(ShapeTypeFromString(inJson["ShapeType"].get<std::string>())); // Collider形状を復元する。
		}

		if (inJson.contains("HalfSize") && inJson["HalfSize"].is_array())
		{
			SetHalfSize(ReadVector3FromJson(inJson, "HalfSize", halfSize_)); // AABBなどで使う半径サイズを復元する。
		}

		if (inJson.contains("IsTrigger") && inJson["IsTrigger"].is_boolean())
		{
			SetIsTrigger(inJson["IsTrigger"].get<bool>()); // Trigger設定を復元する。
		}

		if (inJson.contains("CollisionLayer") && inJson["CollisionLayer"].is_number_unsigned())
		{
			const uint32_t layer = inJson["CollisionLayer"].get<uint32_t>();
			SetCollisionLayer(static_cast<PhysicsCollisionLayer>(layer)); // 衝突レイヤーを復元する。
		}
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