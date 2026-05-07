#include "PlayerHurtboxComponent.h"

#include "Player.h"
#include "CollisionManager.h"
#include <CollisionTypeIdDef.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

static void ExtractAxes_Row_HB(const K4E::Matrix4x4& R, K4E::Vector3& ax, K4E::Vector3& ay, K4E::Vector3& az)
{
	ax = { R.m[0][0], R.m[0][1], R.m[0][2] };
	ay = { R.m[1][0], R.m[1][1], R.m[1][2] };
	az = { R.m[2][0], R.m[2][1], R.m[2][2] };
	ax = K4E::Vector3::Normalize(ax);
	ay = K4E::Vector3::Normalize(ay);
	az = K4E::Vector3::Normalize(az);
}

void PlayerHurtboxComponent::Initialize(Player& owner, CollisionManager* collisionManager)
{
	const uint32_t hurtType = static_cast<uint32_t>(CollisionTypeIdDef::kPlayer);

	auto make = [&](int idx, PlayerHitPart part, float mul, K4E::Vector3 half)
		{
			hurtboxes_[idx] = std::make_unique<PlayerHurtbox>();
			hurtboxes_[idx]->Initialize(&owner, part, mul, hurtType);
			hurtboxes_[idx]->SetOBBHalfSize(half);

			auto& t = tuning_[idx];
			t.halfSize = half;
			t.damageMul = mul;
			t.enabled = true;

			if (t.localOffset.x == 0.0f && t.localOffset.y == 0.0f && t.localOffset.z == 0.0f)
			{
				switch (part)
				{
				case PlayerHitPart::Body: t.localOffset = { 0.0f, 0.0f, 0.0f }; break;
				case PlayerHitPart::Head: t.localOffset = { 0.0f, t.halfSize.y, 0.0f }; break;
				case PlayerHitPart::LeftArm:
				case PlayerHitPart::RightArm:
				case PlayerHitPart::LeftLeg:
				case PlayerHitPart::RightLeg:
					t.localOffset = { 0.0f, -t.halfSize.y, 0.0f };
					break;
				default:
					t.localOffset = { 0.0f, 0.0f, 0.0f };
					break;
				}
			}
		};

	make(0, PlayerHitPart::Body, 1.0f, { 0.5f, 0.75f, 0.25f });
	make(1, PlayerHitPart::Head, 2.0f, { 0.5f, 0.5f, 0.5f });
	make(2, PlayerHitPart::LeftArm, 1.0f, { 0.25f, 0.75f, 0.25f });
	make(3, PlayerHitPart::RightArm, 1.0f, { 0.25f, 0.75f, 0.25f });
	make(4, PlayerHitPart::LeftLeg, 1.0f, { 0.25f, 0.75f, 0.25f });
	make(5, PlayerHitPart::RightLeg, 1.0f, { 0.25f, 0.75f, 0.25f });

	if (collisionManager)
	{
		for (auto& hb : hurtboxes_)
		{
			collisionManager->AddCollider(hb.get());
		}
	}
}

void PlayerHurtboxComponent::Sync(Player& owner)
{
	auto apply = [&](int hbIdx, const K4E::Vector3& pivotWorld, const K4E::Vector3& worldRotEuler)
		{
			auto* hb = hurtboxes_[hbIdx].get();
			if (!hb)
			{
				return;
			}

			auto& t = tuning_[hbIdx];
			if (!t.enabled)
			{
				hb->SetOBBHalfSize({ 0,0,0 });
				hb->ClearOBBBasis();
				return;
			}

			hb->SetOBBHalfSize(t.halfSize);
			const K4E::Vector3 rotOBB = worldRotEuler + t.rotOffset;
			const K4E::Matrix4x4 R = K4E::Matrix4x4::MakeRotateMatrix(rotOBB);

			K4E::Vector3 ax, ay, az;
			ExtractAxes_Row_HB(R, ax, ay, az);

			const K4E::Vector3 center = pivotWorld + ax * t.localOffset.x + ay * t.localOffset.y + az * t.localOffset.z;
			hb->SetCenterPosition(center);
			hb->SetOBBBasis(ax, ay, az);
			hb->SetOrientation(rotOBB);
		};

	auto applyTransform = [&](int hbIdx, const K4E::WorldTransformEx& tr)
		{
			auto* hb = hurtboxes_[hbIdx].get();
			if (!hb)
			{
				return;
			}

			auto& t = tuning_[hbIdx];
			if (!t.enabled)
			{
				hb->SetOBBHalfSize({ 0,0,0 });
				hb->ClearOBBBasis();
				return;
			}

			// 腕などの一人称表示では Quaternion で姿勢を作っている。
			// その場合 worldRotate_ は既存互換用の Euler 値なので、
			// コライダーの向きは描画と同じ worldMatrix_ から直接取る。
			if (!tr.useQuaternionRotation_)
			{
				apply(hbIdx, tr.worldTranslate_, tr.worldRotate_);
				return;
			}

			hb->SetOBBHalfSize(t.halfSize);

			K4E::Vector3 ax, ay, az;
			ExtractAxes_Row_HB(tr.worldMatrix_, ax, ay, az);

			// チューニング用の回転オフセットがある場合だけ、行列から取った軸に追加で回転を掛ける。
			const bool hasRotOffset =
				(t.rotOffset.x != 0.0f) ||
				(t.rotOffset.y != 0.0f) ||
				(t.rotOffset.z != 0.0f);

			if (hasRotOffset)
			{
				const K4E::Matrix4x4 offsetR = K4E::Matrix4x4::MakeRotateMatrix(t.rotOffset);
				const K4E::Matrix4x4 basisR =
				{
					ax.x, ax.y, ax.z, 0.0f,
					ay.x, ay.y, ay.z, 0.0f,
					az.x, az.y, az.z, 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f,
				};
				const K4E::Matrix4x4 fixedR = K4E::Matrix4x4::Multiply(offsetR, basisR);
				ExtractAxes_Row_HB(fixedR, ax, ay, az);
			}

			const K4E::Vector3 center =
				tr.worldTranslate_ +
				ax * t.localOffset.x +
				ay * t.localOffset.y +
				az * t.localOffset.z;

			hb->SetCenterPosition(center);
			hb->SetOBBBasis(ax, ay, az);
			hb->SetOrientation(tr.worldRotate_ + t.rotOffset);
		};

	auto* bodyTr = owner.GetWorldTransform();
	if (!bodyTr)
	{
		return;
	}

	apply(0, bodyTr->translate_, bodyTr->rotate_);

	const auto idx = owner.GetPartIndices();
	auto& parts = owner.GetBodyParts();
	applyTransform(1, parts[idx.head].transform);
	applyTransform(2, parts[idx.leftArm].transform);
	applyTransform(3, parts[idx.rightArm].transform);
	applyTransform(4, parts[idx.leftLeg].transform);
	applyTransform(5, parts[idx.rightLeg].transform);
}

void PlayerHurtboxComponent::DrawImGui()
{
#ifdef USE_IMGUI
	static const char* kPartNames[] = { "Body", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg" };
	if (ImGui::CollapsingHeader("Player Hurtbox Tuning", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("DebugDraw", &debugDraw_);
		ImGui::Combo("Part", &selected_, kPartNames, IM_ARRAYSIZE(kPartNames));

		auto& t = tuning_[selected_];
		ImGui::Checkbox("Enabled", &t.enabled);
		ImGui::DragFloat3("LocalOffset", &t.localOffset.x, 0.01f);
		ImGui::DragFloat3("HalfSize", &t.halfSize.x, 0.01f, 0.01f, 10.0f);
		ImGui::DragFloat3("RotOffset", &t.rotOffset.x, 0.01f);
		ImGui::DragFloat("DamageMul", &t.damageMul, 0.01f, 0.1f, 10.0f);
	}
#endif
}
