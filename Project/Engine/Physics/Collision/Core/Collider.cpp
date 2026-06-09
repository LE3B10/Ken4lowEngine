#define NOMINMAX
#include "Collider.h"
#include <Wireframe.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///					　	OBBを取得
	/// -------------------------------------------------------------
	OBB Collider::GetOBB() const
	{
		return shapeInfo_.BuildOBB(); // 既存OBB生成をCollisionShapeInfoへ委譲する。
	}

	void Collider::SetOBBBasis(const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ)
	{
		shapeInfo_.obbBasis[0] = Vector3::Normalize(axisX);
		shapeInfo_.obbBasis[1] = Vector3::Normalize(axisY);
		shapeInfo_.obbBasis[2] = Vector3::Normalize(axisZ);
		shapeInfo_.useOBBBasis = true;
	}

	void Collider::ClearOBBBasis()
	{
		shapeInfo_.useOBBBasis = false;
	}

	/// -------------------------------------------------------------
	///					  衝突状態フレーム開始
	/// -------------------------------------------------------------
	void Collider::BeginCollisionFrame()
	{
		eventState_.BeginFrame(); // 接触履歴更新をCollisionEventStateへ委譲する。
	}

	/// -------------------------------------------------------------
	///					  このフレームでの接触登録
	/// -------------------------------------------------------------
	void Collider::AddCollisionThisFrame(uint32_t otherUniqueId)
	{
		eventState_.AddContact(otherUniqueId); // 接触登録をCollisionEventStateへ委譲する。
	}

	/// -------------------------------------------------------------
	///					　	初期化処理
	/// -------------------------------------------------------------
	void Collider::Initialize()
	{

	}

	/// -------------------------------------------------------------
	///					　	 更新処理
	/// -------------------------------------------------------------
	void Collider::Update()
	{

	}

	/// -------------------------------------------------------------
	///					　	 描画処理
	/// -------------------------------------------------------------
	void Collider::Draw()
	{
		// 半サイズが 0 に非常に近いなら OBB を描画しない（≒ 無効とみなす）
		if (shapeInfo_.HasDrawableOBB())
		{
			const OBB obb = GetOBB();
			Wireframe::GetInstance()->DrawOBB(obb, shapeInfo_.debugColor);
		}

		// 線分の長さが十分なら描画（セグメントが有効なら）
		if (shapeInfo_.HasDrawableSegment())
		{
			Wireframe::GetInstance()->DrawSegment(shapeInfo_.segment, shapeInfo_.debugColor);
		}

		// -------- Capsule 描画 -------- //
		Vector3 axis = shapeInfo_.capsule.GetAxis();
		if (shapeInfo_.HasDrawableCapsule())
		{
			if (Vector3::Length(axis) < 1e-6f)
				Wireframe::GetInstance()->DrawSphere(shapeInfo_.capsule.segment.origin, shapeInfo_.capsule.radius, shapeInfo_.debugColor);
			else
				Wireframe::GetInstance()->DrawCapsule(shapeInfo_.capsule.GetCenter(), shapeInfo_.capsule.radius, shapeInfo_.capsule.GetHeight(), axis, 8, shapeInfo_.debugColor);
		}
	}

	/// -------------------------------------------------------------
	///					　	 ImGui描画処理
	/// -------------------------------------------------------------
	void Collider::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::TreeNode("Collider")) {
			Vector3 pos = shapeInfo_.colliderPosition;
			if (ImGui::DragFloat3("Center", &pos.x, 0.1f)) {
				SetCenterPosition(pos);
			}

			Vector3 size = shapeInfo_.colliderHalfSize;
			if (ImGui::DragFloat3("HalfSize", &size.x, 0.1f)) {
				SetOBBHalfSize(size);
			}

			Vector3 rot = shapeInfo_.orientation;
			if (ImGui::DragFloat3("Orientation", &rot.x, 0.1f)) {
				SetOrientation(rot);
			}

			ImGui::ColorEdit4("Color", &shapeInfo_.debugColor.x);

			// -------- Capsule -------- //
			if (ImGui::Checkbox("Use Capsule", &shapeInfo_.useCapsule)) {}
			if (ImGui::Checkbox("Draw Capsule", &shapeInfo_.drawCapsule)) {}
			if (shapeInfo_.useCapsule)
			{
				// Segment::diff は「終点への差分(end - origin)」として扱う
				Vector3 pA = shapeInfo_.capsule.segment.origin;
				Vector3 pB = shapeInfo_.capsule.segment.origin + shapeInfo_.capsule.segment.diff;
				float   r = shapeInfo_.capsule.radius;

				bool changedA = ImGui::DragFloat3("Point A", &pA.x, 0.05f);
				bool changedB = ImGui::DragFloat3("Point B", &pB.x, 0.05f);

				if (changedA)
				{
					// Aを動かしたときはBを固定したいので diff を更新
					shapeInfo_.capsule.segment.origin = pA;
					shapeInfo_.capsule.segment.diff = pB - pA;
				}
				if (changedB && !changedA)
				{
					// Bのみ変更された場合
					shapeInfo_.capsule.segment.diff = pB - shapeInfo_.capsule.segment.origin;
				}

				if (ImGui::DragFloat("Radius", &r, 0.01f))
				{
					shapeInfo_.capsule.radius = std::max(0.0f, r);
				}
			}

			ImGui::TreePop();
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
