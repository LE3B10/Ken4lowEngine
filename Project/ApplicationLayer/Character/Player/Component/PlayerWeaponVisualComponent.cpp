#include "PlayerWeaponVisualComponent.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	std::string NormalizeWeaponModelPath(std::string& path)
	{
		if (path.empty())
		{
			return path;
		}

		// \ を / に統一
		std::replace(path.begin(), path.end(), '\\', '/');

		// ModelPathResolver 側で Resources/Models/Sources を前に付けるため、
		// 武器データ側の modelPath は「Weapons/xxx.gltf」のような論理パスに揃える。
		// ここで Sources を残すと
		// Resources/Models/Sources/Sources/Weapons/xxx.gltf
		// のように二重になり、モデルを読めなくなる。
		const std::string prefixes[] =
		{
			"Resources/Models/Sources/",
			"Resources/Models/Compiled/",
			"Resources/Models/",
			"Models/Sources/",
			"Models/Compiled/",
			"Sources/Models/",
			"Sources/",
			"Compiled/",
		};

		for (const std::string& prefix : prefixes)
		{
			const size_t pos = path.find(prefix);
			if (pos != std::string::npos)
			{
				path = path.substr(pos + prefix.size());
				break;
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
	const K4E::Vector3 localRot = isADS ? adsLocalRotate_ : hipLocalRotate_;

	const K4E::Vector3 totalLocalOffset = localPos + handSocketLocalOffset_;
	const K4E::Vector3 totalLocalRotate = localRot + handSocketLocalRotate_;
	const K4E::Vector3 weaponWorldScale = modelScale_ * viewModelScaleMultiplier_;

	// 位置・回転を別々のEuler角に戻すと、モデルの90度補正とカメラピッチが噛み合わず、
	// 上下を向いた時に武器が横回転へ逃げていた。
	// ここでは「武器ローカル補正 → 右腕ワールド行列」をそのまま使い、行列のまま描画へ渡す。
	const K4E::Matrix4x4 localMatrix = K4E::Matrix4x4::MakeAffineMatrix(
		weaponWorldScale,
		totalLocalRotate,
		totalLocalOffset);

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
