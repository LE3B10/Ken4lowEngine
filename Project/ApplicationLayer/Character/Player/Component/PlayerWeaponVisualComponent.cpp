#include "PlayerWeaponVisualComponent.h"

#include <algorithm>

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

		// "Resources/Models/" が含まれていたらそこを削る
		static const std::string kPrefix = "Resources/Models/";
		size_t pos = path.find(kPrefix);
		if (pos != std::string::npos)
		{
			path = path.substr(pos + kPrefix.size());
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
	newObject->SetScale(modelScale_ * 0.5f);

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

	const K4E::Matrix4x4 localMatrix = K4E::Matrix4x4::MakeAffineMatrix(
		modelScale_ * 0.5f,
		totalLocalRotate,
		totalLocalOffset);

	weaponWorldMatrix_ = K4E::Matrix4x4::Multiply(localMatrix, rightHandTransform_->worldMatrix_);
	hasWeaponWorldMatrix_ = true;

	K4E::Vector3 worldScale{};
	K4E::Vector3 worldRotate{};
	K4E::Vector3 worldTranslate{};
	K4E::Matrix4x4::Decompose(weaponWorldMatrix_, worldScale, worldRotate, worldTranslate);

	weaponObject_->SetScale(worldScale);
	weaponObject_->SetRotate(worldRotate);
	weaponObject_->SetTranslate(worldTranslate);
	weaponObject_->Update();
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
	obj->SetScale(modelScale_ * 0.2f);
	weaponObject_ = std::move(obj);
	return true;
}

K4E::Vector3 PlayerWeaponVisualComponent::TransformWeaponLocalPoint(const K4E::Vector3& localPoint) const
{
	return K4E::Matrix4x4::Transform(localPoint, weaponWorldMatrix_);
}
