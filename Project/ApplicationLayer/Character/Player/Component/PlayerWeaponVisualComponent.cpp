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

	void ExtractAxes_Row(const K4E::Matrix4x4& R, K4E::Vector3& ax, K4E::Vector3& ay, K4E::Vector3& az)
	{
		ax = { R.m[0][0], R.m[0][1], R.m[0][2] };
		ay = { R.m[1][0], R.m[1][1], R.m[1][2] };
		az = { R.m[2][0], R.m[2][1], R.m[2][2] };
		ax = K4E::Vector3::Normalize(ax);
		ay = K4E::Vector3::Normalize(ay);
		az = K4E::Vector3::Normalize(az);
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

	const K4E::Vector3 handPos = rightHandTransform_->worldTranslate_;
	const K4E::Vector3 handRot = rightHandTransform_->worldRotate_;

	const K4E::Vector3 localPos = isADS ? adsLocalOffset_ : hipLocalOffset_;
	const K4E::Vector3 localRot = isADS ? adsLocalRotate_ : hipLocalRotate_;

	const K4E::Vector3 totalLocalOffset = localPos + handSocketLocalOffset_;
	const K4E::Vector3 totalLocalRotate = localRot + handSocketLocalRotate_;

	// 前回の localMatrix * rightHandWorldMatrix 方式だと、行列の向きと
	// Object3D 側のオイラー角復元の相性で上下左右が反転していた。
	// ここでは既存の右手 worldTranslate/worldRotate を基準に戻し、
	// 位置だけ右手の回転軸でローカル→ワールド変換する。
	const K4E::Matrix4x4 handRotMatrix = K4E::Matrix4x4::MakeRotateMatrix(handRot);

	K4E::Vector3 ax, ay, az;
	ExtractAxes_Row(handRotMatrix, ax, ay, az);

	const K4E::Vector3 weaponWorldPos =
		handPos +
		ax * totalLocalOffset.x +
		ay * totalLocalOffset.y +
		az * totalLocalOffset.z;

	const K4E::Vector3 weaponWorldRot = handRot + totalLocalRotate;
	const K4E::Vector3 weaponWorldScale = modelScale_ * 0.5f;

	weaponObject_->SetScale(weaponWorldScale);
	weaponObject_->SetRotate(weaponWorldRot);
	weaponObject_->SetTranslate(weaponWorldPos);
	weaponObject_->Update();

	weaponWorldMatrix_ = K4E::Matrix4x4::MakeAffineMatrix(
		weaponWorldScale,
		weaponWorldRot,
		weaponWorldPos);
	hasWeaponWorldMatrix_ = true;
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
