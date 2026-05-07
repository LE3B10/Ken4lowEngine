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

		// Matrix4x4::MakeRotateMatrix(Vector3) の X -> Y -> Z と同じ考え方で合成する。
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

		// \ を / に統一
		std::replace(path.begin(), path.end(), '\\', '/');

		// モデル読み込み側では ModelPathResolver が必ず
		// Resources/Models/Sources/ を前に付ける。
		// そのため、ここで返す値は Weapons/xxx.gltf のような論理パスにする。
		// 例:
		//   Resources/Models/Sources/Weapons/primary_rifle.gltf -> Weapons/primary_rifle.gltf
		//   Models/Sources/Weapons/primary_rifle.gltf           -> Weapons/primary_rifle.gltf
		//   Sources/Models/Weapons/primary_rifle.gltf           -> Weapons/primary_rifle.gltf
		// 最終的な実ファイルパスは ModelPathResolver 側で
		// Resources/Models/Sources/Weapons/primary_rifle.gltf になる。
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

		// 念のため、先頭に残った / を落とす
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
}

void PlayerWeaponVisualComponent::Update(float deltaTime, bool isADS)
{
	(void)deltaTime; // 現状は未使用。将来的にアニメーションの更新などで使うかも。

	RebuildIfWeaponChanged();
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

		if (ImGui::Button("Reset Weapon Size"))
		{
			modelScale_ = { 0.55f, 0.55f, 0.55f };
			viewModelScaleMultiplier_ = 0.55f;
		}
	}
#endif
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

	// 武器なしなら安全に見た目だけ消す
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

	// 新しいオブジェクトを先に作る
	auto newObject = std::make_unique<K4E::Object3D>();
	newObject->Initialize(relativePath);
	newObject->SetScale(modelScale_ * viewModelScaleMultiplier_);

	// 成功後に差し替える
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
	const K4E::Vector3 totalLocalOffset = localPos + handSocketLocalOffset_;

	K4E::Matrix4x4 localMatrix{};
	if (useQuaternionRotation_)
	{
		if (!rotationQuaternionInitialized_)
		{
			InitializeRotationQuaternionsFromEuler();
		}

		const K4E::Quaternion baseRotation = isADS ? adsLocalQuaternion_ : hipLocalQuaternion_;
		const K4E::Quaternion totalRotation = K4E::Quaternion::Normalize(
			K4E::Quaternion::Multiply(baseRotation, handSocketLocalQuaternion_));

		localMatrix = K4E::Matrix4x4::MakeAffineMatrix(
			weaponWorldScale,
			totalRotation,
			totalLocalOffset);
	}
	else
	{
		const K4E::Vector3 localRot = isADS ? adsLocalRotate_ : hipLocalRotate_;
		const K4E::Vector3 totalLocalRotate = localRot + handSocketLocalRotate_;

		localMatrix = K4E::Matrix4x4::MakeAffineMatrix(
			weaponWorldScale,
			totalLocalRotate,
			totalLocalOffset);
	}

	// 右腕ワールド行列へ、武器ローカル補正をそのまま合成する。
	// 回転をEuler角に戻さないことで、ジンバルロックや軸の逃げを避ける。
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
