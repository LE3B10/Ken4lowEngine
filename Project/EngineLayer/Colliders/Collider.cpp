#define NOMINMAX
#include "Collider.h"
#include <Wireframe.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	static void ExtractAxes_Row(const Matrix4x4& R, Vector3& ax, Vector3& ay, Vector3& az)
	{
		ax = { R.m[0][0], R.m[0][1], R.m[0][2] };
		ay = { R.m[1][0], R.m[1][1], R.m[1][2] };
		az = { R.m[2][0], R.m[2][1], R.m[2][2] };
		ax = Vector3::Normalize(ax);
		ay = Vector3::Normalize(ay);
		az = Vector3::Normalize(az);
	}

	/// -------------------------------------------------------------
	///					　	OBBを取得
	/// -------------------------------------------------------------
	OBB Collider::GetOBB() const
	{
		OBB obb{};
		obb.center = colliderPosition_;
		obb.size = colliderHalfSize_;

		if (useOBBBasis)
		{
			obb.orientations[0] = obbBasis_[0];
			obb.orientations[1] = obbBasis_[1];
			obb.orientations[2] = obbBasis_[2];
			return obb;
		}

		Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(orientation_);
		Vector3 axisX, axisY, axisZ;
		ExtractAxes_Row(rotMat, axisX, axisY, axisZ);

		// 回転行列から各軸ベクトルを抽出して OBB に設定
		obb.orientations[0] = axisX; // X軸
		obb.orientations[1] = axisY; // Y軸
		obb.orientations[2] = axisZ; // Z軸

		return obb;
	}

	void Collider::SetOBBBasis(const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ)
	{
		obbBasis_[0] = Vector3::Normalize(axisX);
		obbBasis_[1] = Vector3::Normalize(axisY);
		obbBasis_[2] = Vector3::Normalize(axisZ);
		useOBBBasis = true;
	}

	void Collider::ClearOBBBasis()
	{
		useOBBBasis = false;
	}

	/// -------------------------------------------------------------
	///					  衝突状態フレーム開始
	/// -------------------------------------------------------------
	void Collider::BeginCollisionFrame()
	{
		// prev <- current, current をクリア
		prevCollisions_.swap(currentCollisions_);
		currentCollisions_.clear();
	}

	/// -------------------------------------------------------------
	///					  このフレームでの接触登録
	/// -------------------------------------------------------------
	void Collider::AddCollisionThisFrame(uint32_t otherUniqueId)
	{
		currentCollisions_.insert(otherUniqueId);
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
		if (colliderHalfSize_.x > 0.001f || colliderHalfSize_.y > 0.001f || colliderHalfSize_.z > 0.001f)
		{
			const OBB obb = GetOBB();
			Wireframe::GetInstance()->DrawOBB(obb, debugColor_);
		}

		// 線分の長さが十分なら描画（セグメントが有効なら）
		if (Vector3::Length(segment_.diff) > 0.001f)
		{
			Wireframe::GetInstance()->DrawSegment(segment_, debugColor_);
		}

		// -------- Capsule 描画 -------- //
		Vector3 axis = capsule_.GetAxis();
		if (useCapsule_ && drawCapsule_ && capsule_.radius > 0.001f)
		{
			if (Vector3::Length(axis) < 1e-6f)
				Wireframe::GetInstance()->DrawSphere(capsule_.segment.origin, capsule_.radius, debugColor_);
			else
				Wireframe::GetInstance()->DrawCapsule(capsule_.GetCenter(), capsule_.radius, capsule_.GetHeight(), axis, 8, debugColor_);
		}
	}

	/// -------------------------------------------------------------
	///					　	 ImGui描画処理
	/// -------------------------------------------------------------
	void Collider::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::TreeNode("Collider")) {
			Vector3 pos = colliderPosition_;
			if (ImGui::DragFloat3("Center", &pos.x, 0.1f)) {
				SetCenterPosition(pos);
			}

			Vector3 size = colliderHalfSize_;
			if (ImGui::DragFloat3("HalfSize", &size.x, 0.1f)) {
				SetOBBHalfSize(size);
			}

			Vector3 rot = orientation_;
			if (ImGui::DragFloat3("Orientation", &rot.x, 0.1f)) {
				SetOrientation(rot);
			}

			ImGui::ColorEdit4("Color", &debugColor_.x);

			// -------- Capsule -------- //
			if (ImGui::Checkbox("Use Capsule", &useCapsule_)) {}
			if (ImGui::Checkbox("Draw Capsule", &drawCapsule_)) {}
			if (useCapsule_)
			{
				// Segment::diff は「終点への差分(end - origin)」として扱う
				Vector3 pA = capsule_.segment.origin;
				Vector3 pB = capsule_.segment.origin + capsule_.segment.diff;
				float   r = capsule_.radius;

				bool changedA = ImGui::DragFloat3("Point A", &pA.x, 0.05f);
				bool changedB = ImGui::DragFloat3("Point B", &pB.x, 0.05f);

				if (changedA)
				{
					// Aを動かしたときはBを固定したいので diff を更新
					capsule_.segment.origin = pA;
					capsule_.segment.diff = pB - pA;
				}
				if (changedB && !changedA)
				{
					// Bのみ変更された場合
					capsule_.segment.diff = pB - capsule_.segment.origin;
				}

				if (ImGui::DragFloat("Radius", &r, 0.01f))
				{
					capsule_.radius = std::max(0.0f, r);
				}
			}

			ImGui::TreePop();
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
