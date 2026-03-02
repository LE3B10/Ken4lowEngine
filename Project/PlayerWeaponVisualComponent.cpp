#include "PlayerWeaponVisualComponent.h"

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

	K4E::Vector3 RotateByEulerLocal(const K4E::Vector3& v, const K4E::Vector3& rot)
	{
		const auto m = K4E::Matrix4x4::MakeRotateMatrix(rot);
		return {
			m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
			m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
			m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z,
		};
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
}

void PlayerWeaponVisualComponent::Initialize()
{
	weaponObject_.reset();
	appliedWeaponId_ = 0;
	visible_ = true;
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
	if (!weaponObject_ || !rightHandTransform_) return;

	const K4E::Vector3 handPos = rightHandTransform_->worldTranslate_;
	const K4E::Vector3 handRot = rightHandTransform_->worldRotate_;

	const K4E::Vector3 localPos = isADS ? adsLocalOffset_ : hipLocalOffset_;
	const K4E::Vector3 localRot = isADS ? adsLocalRotate_ : hipLocalRotate_;

	// 右手の回転行列
	const K4E::Matrix4x4 R = K4E::Matrix4x4::MakeRotateMatrix(handRot);

	K4E::Vector3 ax, ay, az;
	ExtractAxes_Row(R, ax, ay, az);

	// ローカルオフセットを右手基準でワールド化
	const K4E::Vector3 weaponWorldPos =
		handPos +
		ax * localPos.x +
		ay * localPos.y +
		az * localPos.z;

	weaponObject_->SetTranslate(weaponWorldPos);

	// まずは仮でそのまま
	weaponObject_->SetRotate(handRot + localRot);

	weaponObject_->Update();
}

bool PlayerWeaponVisualComponent::LoadWeaponModel(const std::string& modelPath)
{
	weaponObject_.reset();

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
