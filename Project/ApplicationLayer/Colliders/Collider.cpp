#define NOMINMAX
#include "Collider.h"
#include <Wireframe.h>

#include <imgui.h>

/// -------------------------------------------------------------
///						　	OBBを取得
/// -------------------------------------------------------------
OBB Collider::GetOBB() const
{
	Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(orientation_);

	OBB obb{};
	obb.center = colliderPosition_;
	obb.size = colliderHalfSize_;

	// 回転行列から各軸ベクトルを抽出して OBB に設定
	obb.orientations[0] = { rotMat.m[0][0], rotMat.m[1][0], rotMat.m[2][0] }; // X軸
	obb.orientations[1] = { rotMat.m[0][1], rotMat.m[1][1], rotMat.m[2][1] }; // Y軸
	obb.orientations[2] = { rotMat.m[0][2], rotMat.m[1][2], rotMat.m[2][2] }; // Z軸

	return obb;
}


/// -------------------------------------------------------------
///						　	初期化処理
/// -------------------------------------------------------------
void Collider::Initialize()
{

}


/// -------------------------------------------------------------
///						　	 更新処理
/// -------------------------------------------------------------
void Collider::Update()
{

}


/// -------------------------------------------------------------
///						　	 描画処理
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
///						　	 ImGui描画処理
/// -------------------------------------------------------------
void Collider::DrawImGui()
{
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
		if (useCapsule_)
		{
			Vector3 pA = capsule_.segment.origin;
			Vector3 pB = capsule_.segment.diff;
			float   r = capsule_.radius;

			if (ImGui::DragFloat3("Point A", &pA.x, 0.05f)) capsule_.segment.origin = pA;
			if (ImGui::DragFloat3("Point B", &pB.x, 0.05f)) capsule_.segment.diff = pB;
			if (ImGui::DragFloat("Radius", &r, 0.01f))  capsule_.radius = std::max(0.0f, r);
		}

		ImGui::TreePop();
	}
}
