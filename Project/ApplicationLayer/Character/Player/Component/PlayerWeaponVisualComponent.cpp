#define NOMINMAX
#include "PlayerWeaponVisualComponent.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	float ToRadians(float degrees)
	{
		return degrees * (kPi / 180.0f);
	}

	float ToDegrees(float radians)
	{
		return radians * (180.0f / kPi);
	}

	float Clamp01(float v)
	{
		return std::clamp(v, 0.0f, 1.0f);
	}

	float SmoothStep01(float t)
	{
		t = Clamp01(t);
		return t * t * (3.0f - 2.0f * t);
	}

	float Approach(float current, float target, float speed, float deltaTime)
	{
		const float step = speed * deltaTime;
		const float diff = target - current;
		if (diff > step) return current + step;
		if (diff < -step) return current - step;
		return target;
	}

	K4E::Vector3 ToDegreesVec(const K4E::Vector3& radians)
	{
		return { ToDegrees(radians.x), ToDegrees(radians.y), ToDegrees(radians.z) };
	}

	K4E::Quaternion MakeQuaternionFromEulerDeg(const K4E::Vector3& eulerDeg)
	{
		const float x = ToRadians(eulerDeg.x);
		const float y = ToRadians(eulerDeg.y);
		const float z = ToRadians(eulerDeg.z);

		const K4E::Quaternion qx = K4E::Quaternion::MakeRotateAxisAngleQuaternion({ 1.0f, 0.0f, 0.0f }, x);
		const K4E::Quaternion qy = K4E::Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f, 1.0f, 0.0f }, y);
		const K4E::Quaternion qz = K4E::Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f, 0.0f, 1.0f }, z);

		return K4E::Quaternion::Normalize(
			K4E::Quaternion::Multiply(
				K4E::Quaternion::Multiply(qx, qy),
				qz));
	}

	std::string NormalizeWeaponModelPath(std::string& path)
	{
		if (path.empty())
		{
			return path;
		}

		std::replace(path.begin(), path.end(), '\\', '/');

		const std::string sourceRoot = "Resources/Models/Sources/";
		const size_t sourceRootPos = path.find(sourceRoot);
		if (sourceRootPos != std::string::npos)
		{
			path = path.substr(sourceRootPos + sourceRoot.size());
		}
		else
		{
			const std::string removablePrefixes[] =
			{
				"Models/Sources/",
				"Sources/Models/",
				"Resources/Models/",
				"Models/",
				"Sources/",
			};

			for (const std::string& prefix : removablePrefixes)
			{
				if (path.rfind(prefix, 0) == 0)
				{
					path = path.substr(prefix.size());
					break;
				}
			}
		}

		while (!path.empty() && path.front() == '/')
		{
			path.erase(path.begin());
		}

		return path;
	}

	K4E::Vector3 ExtractForwardFromMatrix(const K4E::Matrix4x4& m)
	{
		K4E::Vector3 forward{ m.m[2][0], m.m[2][1], m.m[2][2] };
		const float lenSq =
			forward.x * forward.x +
			forward.y * forward.y +
			forward.z * forward.z;

		if (lenSq <= 0.0001f)
		{
			return { 0.0f, 0.0f, 1.0f };
		}

		return K4E::Vector3::Normalize(forward);
	}
}

void PlayerWeaponVisualComponent::Initialize()
{
	weaponObject_.reset();
	appliedWeaponId_ = 0;
	visible_ = true;
	hasWeaponWorldMatrix_ = false;
	weaponWorldMatrix_ = K4E::Matrix4x4::MakeIdentity();
	rotationQuaternionInitialized_ = false;
	reloadViewActive_ = false;
	reloadViewTimer_ = 0.0f;
	reloadViewDuration_ = 1.0f;
	reloadPoseAlpha_ = 0.0f;
}

void PlayerWeaponVisualComponent::Update(float deltaTime, bool isADS)
{
	RebuildIfWeaponChanged();
	if (equipAnimating_)
	{
		equipTimer_ += deltaTime;
		if (equipTimer_ >= equipDuration_)
		{
			equipTimer_ = equipDuration_;
			equipAnimating_ = false;
		}
	}

	const float targetReloadAlpha = reloadViewActive_ ? 1.0f : 0.0f;
	reloadPoseAlpha_ = Approach(reloadPoseAlpha_, targetReloadAlpha, reloadPoseBlendSpeed_, deltaTime);

	SyncToHand(isADS);
}

void PlayerWeaponVisualComponent::Draw()
{
	if (!visible_) return;
	if (!weaponObject_) return;

	weaponObject_->Draw();
}

void PlayerWeaponVisualComponent::DrawShadow()
{
	if (!visible_) return;
	if (!weaponObject_) return;
	//weaponObject_->DrawShadow();
}

void PlayerWeaponVisualComponent::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Weapon Visual", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Visible", &visible_);
		ImGui::DragFloat("View Model Scale", &viewModelScaleMultiplier_, 0.01f, 0.05f, 2.0f, "%.2f");
		ImGui::DragFloat3("Model Scale", &modelScale_.x, 0.01f, 0.05f, 2.0f, "%.2f");
		ImGui::DragFloat3("Hip Offset", &hipLocalOffset_.x, 0.01f, -5.0f, 5.0f, "%.2f");
		ImGui::DragFloat3("ADS Offset", &adsLocalOffset_.x, 0.01f, -5.0f, 5.0f, "%.2f");
		ImGui::DragFloat3("Muzzle Offset", &muzzleLocalOffset_.x, 0.01f, -5.0f, 5.0f, "%.2f");

		ImGui::Separator();
		ImGui::Checkbox("Use Quaternion Rotation", &useQuaternionRotation_);

		if (ImGui::Button("Reset Quaternion From Current Euler"))
		{
			InitializeRotationQuaternionsFromEuler();
		}

		if (useQuaternionRotation_)
		{
			bool changed = false;
			changed |= ImGui::DragFloat3("Hip Rotation Deg", &hipEulerDeg_.x, 0.25f, -360.0f, 360.0f, "%.2f");
			changed |= ImGui::DragFloat3("ADS Rotation Deg", &adsEulerDeg_.x, 0.25f, -360.0f, 360.0f, "%.2f");
			changed |= ImGui::DragFloat3("Hand Socket Rotation Deg", &handSocketEulerDeg_.x, 0.25f, -360.0f, 360.0f, "%.2f");

			if (changed || !rotationQuaternionInitialized_)
			{
				hipLocalQuaternion_ = MakeQuaternionFromEulerDeg(hipEulerDeg_);
				adsLocalQuaternion_ = MakeQuaternionFromEulerDeg(adsEulerDeg_);
				handSocketLocalQuaternion_ = MakeQuaternionFromEulerDeg(handSocketEulerDeg_);
				rotationQuaternionInitialized_ = true;
			}

			ImGui::Text("Hip Q: %.3f, %.3f, %.3f, %.3f", hipLocalQuaternion_.x, hipLocalQuaternion_.y, hipLocalQuaternion_.z, hipLocalQuaternion_.w);
			ImGui::Text("ADS Q: %.3f, %.3f, %.3f, %.3f", adsLocalQuaternion_.x, adsLocalQuaternion_.y, adsLocalQuaternion_.z, adsLocalQuaternion_.w);
			ImGui::Text("Hand Q: %.3f, %.3f, %.3f, %.3f", handSocketLocalQuaternion_.x, handSocketLocalQuaternion_.y, handSocketLocalQuaternion_.z, handSocketLocalQuaternion_.w);
		}
		else
		{
			ImGui::DragFloat3("Hip Rotation Rad", &hipLocalRotate_.x, 0.01f, -6.28f, 6.28f, "%.2f");
			ImGui::DragFloat3("ADS Rotation Rad", &adsLocalRotate_.x, 0.01f, -6.28f, 6.28f, "%.2f");
			ImGui::DragFloat3("Hand Socket Rotation Rad", &handSocketLocalRotate_.x, 0.01f, -6.28f, 6.28f, "%.2f");
		}

		ImGui::Separator();
		ImGui::Text("Reload Pose Alpha: %.2f", reloadPoseAlpha_);
		ImGui::Text("Reload rotation is now controlled by the right arm.");
		ImGui::DragFloat3("Reload Weapon Offset", &reloadWeaponOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat("Reload Blend Speed", &reloadPoseBlendSpeed_, 0.1f, 1.0f, 40.0f, "%.1f");
		ImGui::Separator();
		ImGui::Checkbox("Enable Equip Animation", &enableEquipAnimation_);
		ImGui::DragFloat("Equip Duration", &equipDuration_, 0.01f, 0.05f, 1.0f, "%.2f");
		ImGui::DragFloat("Equip Start Offset Y", &equipStartOffsetY_, 0.01f, -2.0f, 0.0f, "%.2f");
		ImGui::DragFloat("Equip Start Pitch Deg", &equipStartPitchDeg_, 0.5f, -45.0f, 0.0f, "%.1f");
		const float equipNormalizedT = Clamp01((equipDuration_ > 0.001f) ? (equipTimer_ / equipDuration_) : 1.0f);
		const float equipEaseOut = 1.0f - (1.0f - equipNormalizedT) * (1.0f - equipNormalizedT);
		const K4E::Vector3 equipCurrentOffset = { 0.0f, (1.0f - equipEaseOut) * equipStartOffsetY_, 0.0f };
		ImGui::Text("Equip animation enabled: %s", enableEquipAnimation_ ? "true" : "false");
		ImGui::Text("IsEquipAnimating: %s", IsEquipAnimating() ? "true" : "false");
		ImGui::Text("Equip elapsed time: %.3f", equipTimer_);
		ImGui::Text("Equip duration: %.3f", equipDuration_);
		ImGui::Text("Equip normalized t: %.3f", equipNormalizedT);
		ImGui::Text("Equip current offset: (%.3f, %.3f, %.3f)", equipCurrentOffset.x, equipCurrentOffset.y, equipCurrentOffset.z);
		if (ImGui::Button("Play Equip Animation"))
		{
			StartEquipAnimation();
		}

		if (ImGui::Button("Reset Weapon Size"))
		{
			modelScale_ = { 0.55f, 0.55f, 0.55f };
			viewModelScaleMultiplier_ = 0.55f;
		}
	}
#endif
}

void PlayerWeaponVisualComponent::SetReloadViewModelState(bool isReloading, float reloadTimer, float reloadDuration)
{
	reloadViewActive_ = isReloading;
	reloadViewTimer_ = reloadTimer;
	reloadViewDuration_ = std::max(0.01f, reloadDuration);
}

void PlayerWeaponVisualComponent::StartEquipAnimation()
{
	// カメラ演出後や武器切り替え時に、下から構える装備アニメーションを再生する。
	if (!enableEquipAnimation_)
	{
		equipAnimating_ = false;
		equipTimer_ = equipDuration_;
		return;
	}
	equipAnimating_ = true;
	equipTimer_ = 0.0f;
}

bool PlayerWeaponVisualComponent::IsEquipAnimating() const
{
	return enableEquipAnimation_ && equipAnimating_;
}

void PlayerWeaponVisualComponent::ForceRefresh()
{
	refreshRequested_ = true;
}

bool PlayerWeaponVisualComponent::TryGetMuzzleWorldPosition(K4E::Vector3& outPosition) const
{
	if (!weaponObject_ || !hasWeaponWorldMatrix_)
	{
		return false;
	}

	outPosition = TransformWeaponLocalPoint(muzzleLocalOffset_);
	return true;
}

bool PlayerWeaponVisualComponent::TryGetMuzzleForward(K4E::Vector3& outForward) const
{
	if (!weaponObject_ || !hasWeaponWorldMatrix_)
	{
		return false;
	}

	outForward = ExtractForwardFromMatrix(weaponWorldMatrix_);
	return true;
}

void PlayerWeaponVisualComponent::RebuildIfWeaponChanged()
{
	if (!weaponLogic_) return;

	const int32_t currentId = weaponLogic_->GetCurrentWeaponId();

	if (currentId <= 0)
	{
		weaponObject_.reset();
		appliedWeaponId_ = 0;
		appliedModelPath_.clear();
		refreshRequested_ = false;
		hasWeaponWorldMatrix_ = false;
		return;
	}

	const auto& db = weaponLogic_->GetWeaponMasterDatabase();
	const FWeaponMasterData* data = db.FindByID(currentId);
	if (!data)
	{
		weaponObject_.reset();
		appliedWeaponId_ = 0;
		appliedModelPath_.clear();
		refreshRequested_ = false;
		hasWeaponWorldMatrix_ = false;
		return;
	}

	std::string modelPath = data->assetData.modelPath;
	std::string relativePath = NormalizeWeaponModelPath(modelPath);

	if (relativePath.empty())
	{
		weaponObject_.reset();
		appliedWeaponId_ = 0;
		appliedModelPath_.clear();
		refreshRequested_ = false;
		hasWeaponWorldMatrix_ = false;
		return;
	}

	const bool sameId = (currentId == appliedWeaponId_);
	const bool samePath = (relativePath == appliedModelPath_);

	if (!refreshRequested_ && sameId && samePath && weaponObject_)
	{
		return;
	}

	auto newObject = std::make_unique<K4E::Object3D>();
	newObject->Initialize(relativePath);
	newObject->SetScale(modelScale_ * viewModelScaleMultiplier_);

	weaponObject_ = std::move(newObject);
	appliedWeaponId_ = currentId;
	appliedModelPath_ = relativePath;
	refreshRequested_ = false;
}

void PlayerWeaponVisualComponent::SyncToHand(bool isADS)
{
	if (!weaponObject_ || !rightHandTransform_)
	{
		hasWeaponWorldMatrix_ = false;
		return;
	}

	const K4E::Vector3 localPos = isADS ? adsLocalOffset_ : hipLocalOffset_;
	const K4E::Vector3 weaponWorldScale = modelScale_ * viewModelScaleMultiplier_;
	const float reloadT = SmoothStep01(reloadPoseAlpha_);
	const K4E::Vector3 reloadOffset = reloadWeaponOffset_ * reloadT;
	const float equipT = Clamp01((equipDuration_ > 0.001f) ? (equipTimer_ / equipDuration_) : 1.0f);
	const float equipEaseOut = 1.0f - (1.0f - equipT) * (1.0f - equipT);
	const float equipPosY = (1.0f - equipEaseOut) * equipStartOffsetY_;
	const float equipPitch = (1.0f - equipEaseOut) * ToRadians(equipStartPitchDeg_);
	const K4E::Vector3 equipOffset = { 0.0f, equipPosY, 0.0f };
	const K4E::Vector3 totalLocalOffset = localPos + handSocketLocalOffset_ + reloadOffset + equipOffset;

	K4E::Matrix4x4 localMatrix{};
	if (useQuaternionRotation_)
	{
		if (!rotationQuaternionInitialized_)
		{
			InitializeRotationQuaternionsFromEuler();
		}

		const K4E::Quaternion baseRotation = isADS ? adsLocalQuaternion_ : hipLocalQuaternion_;
		const K4E::Quaternion equipPitchQ = K4E::Quaternion::MakeRotateAxisAngleQuaternion({ 1.0f, 0.0f, 0.0f }, equipPitch);
		const K4E::Quaternion totalRotation = K4E::Quaternion::Normalize(
			K4E::Quaternion::Multiply(baseRotation, handSocketLocalQuaternion_));
		const K4E::Quaternion animatedRotation = K4E::Quaternion::Normalize(
			K4E::Quaternion::Multiply(equipPitchQ, totalRotation));

		localMatrix = K4E::Matrix4x4::MakeAffineMatrix(
			weaponWorldScale,
			animatedRotation,
			totalLocalOffset);
	}
	else
	{
		const K4E::Vector3 localRot = isADS ? adsLocalRotate_ : hipLocalRotate_;
		const K4E::Vector3 totalLocalRotate = localRot + handSocketLocalRotate_ + K4E::Vector3{ equipPitch, 0.0f, 0.0f };

		localMatrix = K4E::Matrix4x4::MakeAffineMatrix(
			weaponWorldScale,
			totalLocalRotate,
			totalLocalOffset);
	}

	weaponWorldMatrix_ = K4E::Matrix4x4::Multiply(localMatrix, rightHandTransform_->worldMatrix_);
	hasWeaponWorldMatrix_ = true;

	weaponObject_->UpdateWithWorldMatrix(weaponWorldMatrix_);
}

bool PlayerWeaponVisualComponent::LoadWeaponModel(const std::string& modelPath)
{
	weaponObject_.reset();
	hasWeaponWorldMatrix_ = false;

	if (modelPath.empty())
	{
		return false;
	}

	auto obj = std::make_unique<K4E::Object3D>();
	obj->Initialize(modelPath);
	obj->SetScale(modelScale_ * viewModelScaleMultiplier_);
	weaponObject_ = std::move(obj);
	return true;
}

K4E::Vector3 PlayerWeaponVisualComponent::TransformWeaponLocalPoint(const K4E::Vector3& localPoint) const
{
	return K4E::Matrix4x4::Transform(localPoint, weaponWorldMatrix_);
}

void PlayerWeaponVisualComponent::InitializeRotationQuaternionsFromEuler()
{
	hipEulerDeg_ = ToDegreesVec(hipLocalRotate_);
	adsEulerDeg_ = ToDegreesVec(adsLocalRotate_);
	handSocketEulerDeg_ = ToDegreesVec(handSocketLocalRotate_);

	hipLocalQuaternion_ = MakeQuaternionFromEulerDeg(hipEulerDeg_);
	adsLocalQuaternion_ = MakeQuaternionFromEulerDeg(adsEulerDeg_);
	handSocketLocalQuaternion_ = MakeQuaternionFromEulerDeg(handSocketEulerDeg_);
	rotationQuaternionInitialized_ = true;
}
