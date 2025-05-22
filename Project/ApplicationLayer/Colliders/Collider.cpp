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
	OBB obb = GetOBB();
	Wireframe::GetInstance()->DrawOBB(obb, debugColor_);
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

        ImGui::TreePop();
    }
}
