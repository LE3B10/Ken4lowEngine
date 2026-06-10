#define NOMINMAX
#include "AimTargetDetector.h"

#include "BossBase.h"
#include "Camera.h"
#include "Collider.h"
#include "CollisionManager.h"
#include "CollisionTypeIdDef.h"
#include "EnemyBase.h"
#include "EnemySpawnCrystal.h"
#include "ParameterManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
	constexpr const char* kAimTargetGroup = "AimTarget";
	constexpr float kRayEpsilon = 0.0001f;

	AimTargetDetector::ObjectType ToObjectTypeFromTypeId(uint32_t typeId)
	{
		if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
		{
			return AimTargetDetector::ObjectType::Enemy;
		}
		if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kCrystal))
		{
			return AimTargetDetector::ObjectType::Crystal;
		}
		if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
		{
			return AimTargetDetector::ObjectType::Boss;
		}
		if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kWorld))
		{
			return AimTargetDetector::ObjectType::Obstacle;
		}
		return AimTargetDetector::ObjectType::Other;
	}
}

AimTargetDetector::~AimTargetDetector()
{
	UnregisterParameters();
}

void AimTargetDetector::Initialize()
{
	RegisterParameters();
	ApplyParameters();
	initialized_ = true;
}

void AimTargetDetector::Update(const K4E::Camera& camera, const CollisionManager& collisionManager)
{
	if (!initialized_)
	{
		Initialize();
	}
	ApplyParameters();

	// カメラ中心からRayを作成する処理。HUD照準用なのでカメラ位置と前方だけを使う。
	const K4E::Vector3 origin = camera.GetTranslate();
	const K4E::Vector3 direction = K4E::Vector3::NormalizeSafe(camera.GetForward(), { 0.0f, 0.0f, 1.0f });
	ResetResultForRay(origin, direction);

	TestType(collisionManager, static_cast<uint32_t>(CollisionTypeIdDef::kEnemy), ObjectType::Enemy, origin, direction);
	TestType(collisionManager, static_cast<uint32_t>(CollisionTypeIdDef::kCrystal), ObjectType::Crystal, origin, direction);
	TestType(collisionManager, static_cast<uint32_t>(CollisionTypeIdDef::kBoss), ObjectType::Boss, origin, direction);
	if (enableObstacleLineOfSightCheck_)
	{
		TestType(collisionManager, static_cast<uint32_t>(CollisionTypeIdDef::kWorld), ObjectType::Obstacle, origin, direction);
	}

	result_.isDamageableTarget = IsDamageable(result_.hitObjectType);
	// 障害物が手前にある場合に対象扱いしない処理。最前Hitが障害物ならUI対象から外す。
	result_.isBlockedByObstacle = result_.hit && result_.hitObjectType == ObjectType::Obstacle;
	if (result_.isBlockedByObstacle)
	{
		result_.isDamageableTarget = false;
	}
}

void AimTargetDetector::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Aim Target Detector", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Ray Hit: %s", result_.hit ? "true" : "false");
		ImGui::Text("Hit ObjectType: %s", ToString(result_.hitObjectType));
		ImGui::Text("Hit Distance: %.2f", result_.hitDistance);
		ImGui::Text("Hit Position: %.2f, %.2f, %.2f", result_.hitPosition.x, result_.hitPosition.y, result_.hitPosition.z);
		ImGui::Text("Damageable Target: %s", result_.isDamageableTarget ? "true" : "false");
		ImGui::Text("Blocked By Obstacle: %s", result_.isBlockedByObstacle ? "true" : "false");
		ImGui::Text("HP Bar Target: %s", result_.isDamageableTarget ? ToString(result_.hitObjectType) : "None");
		ImGui::Text("Crosshair Red: %s", HasDamageableTarget() ? "true" : "false");
		ImGui::Text("Ray Origin: %.2f, %.2f, %.2f", debugRayOrigin_.x, debugRayOrigin_.y, debugRayOrigin_.z);
		ImGui::Text("Ray Direction: %.2f, %.2f, %.2f", debugRayDirection_.x, debugRayDirection_.y, debugRayDirection_.z);
		ImGui::Text("Ray Length: %.2f", debugRayLength_);
		ImGui::Text("Debug Draw Requested: %s", aimRayDebugDraw_ ? "true" : "false");
		ImGui::Text("Show HPBar Only When Aimed: %s", showHpBarOnlyWhenAimed_ ? "true" : "false");
		ImGui::Text("Hold Time: %.2f", hpBarVisibleHoldTime_);
	}
#endif
}

EnemyBase* AimTargetDetector::GetTargetEnemy() const
{
	if (!result_.isDamageableTarget || result_.hitObjectType != ObjectType::Enemy)
	{
		return nullptr;
	}
	return static_cast<EnemyBase*>(result_.hitObjectPointer);
}

EnemySpawnCrystal* AimTargetDetector::GetTargetCrystal() const
{
	if (!result_.isDamageableTarget || result_.hitObjectType != ObjectType::Crystal)
	{
		return nullptr;
	}
	return static_cast<EnemySpawnCrystal*>(result_.hitObjectPointer);
}

BossBase* AimTargetDetector::GetTargetBoss() const
{
	if (!result_.isDamageableTarget || result_.hitObjectType != ObjectType::Boss)
	{
		return nullptr;
	}
	return static_cast<BossBase*>(result_.hitObjectPointer);
}

const char* AimTargetDetector::ToString(ObjectType type)
{
	switch (type)
	{
	case ObjectType::Enemy:
		return "Enemy";
	case ObjectType::Crystal:
		return "Crystal";
	case ObjectType::Boss:
		return "Boss";
	case ObjectType::Obstacle:
		return "Obstacle";
	case ObjectType::Other:
		return "Other";
	case ObjectType::None:
	default:
		return "None";
	}
}

void AimTargetDetector::RegisterParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	parameters->CreateGroup(kAimTargetGroup);
	parameters->AddItem(kAimTargetGroup, "aimRayLength", aimRayLength_, 10.0f, 3000.0f);
	parameters->AddItem(kAimTargetGroup, "aimRayDebugDraw", aimRayDebugDraw_);
	parameters->AddItem(kAimTargetGroup, "crosshairTargetColor", crosshairTargetColor_);
	parameters->AddItem(kAimTargetGroup, "crosshairNormalColor", crosshairNormalColor_);
	parameters->AddItem(kAimTargetGroup, "hpBarVisibleHoldTime", hpBarVisibleHoldTime_, 0.0f, 2.0f);
	parameters->AddItem(kAimTargetGroup, "showHpBarOnlyWhenAimed", showHpBarOnlyWhenAimed_);
	parameters->AddItem(kAimTargetGroup, "enableObstacleLineOfSightCheck", enableObstacleLineOfSightCheck_);
	parameters->AddItem(kAimTargetGroup, "targetDetectionRadius", targetDetectionRadius_, 0.0f, 5.0f);
	parameters->SetDisplayName(kAimTargetGroup, "aimRayLength", "照準Ray長");
	parameters->SetDisplayName(kAimTargetGroup, "aimRayDebugDraw", "Rayデバッグ描画");
	parameters->SetDisplayName(kAimTargetGroup, "crosshairTargetColor", "照準対象色");
	parameters->SetDisplayName(kAimTargetGroup, "crosshairNormalColor", "照準通常色");
	parameters->SetDisplayName(kAimTargetGroup, "hpBarVisibleHoldTime", "HPバー保持時間");
	parameters->SetDisplayName(kAimTargetGroup, "showHpBarOnlyWhenAimed", "照準時のみHPバー表示");
	parameters->SetDisplayName(kAimTargetGroup, "enableObstacleLineOfSightCheck", "障害物遮蔽判定");
	parameters->SetDisplayName(kAimTargetGroup, "targetDetectionRadius", "照準判定半径");
	parameters->RegisterParameterApplier(kAimTargetGroup, this, [this]() { ApplyParameters(); });
	parameters->LoadFile(kAimTargetGroup);
}

void AimTargetDetector::UnregisterParameters()
{
	K4E::ParameterManager::GetInstance()->UnregisterParameterApplier(kAimTargetGroup, this);
}

void AimTargetDetector::ApplyParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	aimRayLength_ = std::max(1.0f, parameters->GetValue<float>(kAimTargetGroup, "aimRayLength"));
	aimRayDebugDraw_ = parameters->GetValue<bool>(kAimTargetGroup, "aimRayDebugDraw");
	crosshairTargetColor_ = parameters->GetValue<K4E::Vector4>(kAimTargetGroup, "crosshairTargetColor");
	crosshairNormalColor_ = parameters->GetValue<K4E::Vector4>(kAimTargetGroup, "crosshairNormalColor");
	hpBarVisibleHoldTime_ = std::max(0.0f, parameters->GetValue<float>(kAimTargetGroup, "hpBarVisibleHoldTime"));
	showHpBarOnlyWhenAimed_ = parameters->GetValue<bool>(kAimTargetGroup, "showHpBarOnlyWhenAimed");
	enableObstacleLineOfSightCheck_ = parameters->GetValue<bool>(kAimTargetGroup, "enableObstacleLineOfSightCheck");
	targetDetectionRadius_ = std::max(0.0f, parameters->GetValue<float>(kAimTargetGroup, "targetDetectionRadius"));
}

void AimTargetDetector::ResetResultForRay(const K4E::Vector3& origin, const K4E::Vector3& direction)
{
	result_ = {};
	result_.hitDistance = aimRayLength_;
	debugRayOrigin_ = origin;
	debugRayDirection_ = direction;
	debugRayLength_ = aimRayLength_;
}

void AimTargetDetector::TestType(const CollisionManager& collisionManager, uint32_t typeId, ObjectType objectType, const K4E::Vector3& origin, const K4E::Vector3& direction)
{
	const auto& colliders = collisionManager.GetCollidersByType(typeId);
	for (K4E::Collider* collider : colliders)
	{
		if (!collider || !collider->IsQueryEnabled())
		{
			continue;
		}

		float distance = 0.0f;
		if (!IntersectAABB(origin, direction, *collider, distance))
		{
			continue;
		}

		if (distance < 0.0f || distance > aimRayLength_)
		{
			continue;
		}

		// RayHitを距離順に比較して最も近い対象を選ぶ処理。
		if (!result_.hit || distance < result_.hitDistance)
		{
			result_.hit = true;
			result_.hitDistance = distance;
			result_.hitPosition = origin + direction * distance;
			result_.hitObjectType = objectType;
			result_.hitObjectPointer = collider;
		}
	}
}

bool AimTargetDetector::IntersectAABB(const K4E::Vector3& origin, const K4E::Vector3& direction, const K4E::Collider& collider, float& outDistance) const
{
	K4E::AABB aabb = collider.GetAABB();
	if (targetDetectionRadius_ > 0.0f && IsDamageable(ToObjectTypeFromTypeId(collider.GetTypeID())))
	{
		aabb.min -= K4E::Vector3{ targetDetectionRadius_, targetDetectionRadius_, targetDetectionRadius_ };
		aabb.max += K4E::Vector3{ targetDetectionRadius_, targetDetectionRadius_, targetDetectionRadius_ };
	}

	float tMin = 0.0f;
	float tMax = aimRayLength_;
	const std::array<float, 3> o{ origin.x, origin.y, origin.z };
	const std::array<float, 3> d{ direction.x, direction.y, direction.z };
	const std::array<float, 3> mn{ aabb.min.x, aabb.min.y, aabb.min.z };
	const std::array<float, 3> mx{ aabb.max.x, aabb.max.y, aabb.max.z };

	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(d[axis]) < kRayEpsilon)
		{
			if (o[axis] < mn[axis] || o[axis] > mx[axis])
			{
				return false;
			}
			continue;
		}

		float t1 = (mn[axis] - o[axis]) / d[axis];
		float t2 = (mx[axis] - o[axis]) / d[axis];
		if (t1 > t2)
		{
			std::swap(t1, t2);
		}
		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);
		if (tMin > tMax)
		{
			return false;
		}
	}

	outDistance = tMin;
	return true;
}

bool AimTargetDetector::IsDamageable(ObjectType type) const
{
	return type == ObjectType::Enemy || type == ObjectType::Crystal || type == ObjectType::Boss;
}
