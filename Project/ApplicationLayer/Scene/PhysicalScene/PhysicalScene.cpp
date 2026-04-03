#define NOMINMAX
#include "PhysicalScene.h"
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include <DirectXCommon.h>
#include <GameTimer.h>

#include <Wireframe.h>
#include <numbers>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace K4E = ::Ken4lowEngine;

// row-vector: v' = v * R
static K4E::Vector3 RotateVec_Row(const K4E::Vector3& v, const K4E::Matrix4x4& R)
{
	return {
		v.x * R.m[0][0] + v.y * R.m[1][0] + v.z * R.m[2][0],
		v.x * R.m[0][1] + v.y * R.m[1][1] + v.z * R.m[2][1],
		v.x * R.m[0][2] + v.y * R.m[1][2] + v.z * R.m[2][2],
	};
}

static void ExtractAxes_Row(const K4E::Matrix4x4& R, K4E::Vector3& ax, K4E::Vector3& ay, K4E::Vector3& az)
{
	ax = { R.m[0][0], R.m[0][1], R.m[0][2] };
	ay = { R.m[1][0], R.m[1][1], R.m[1][2] };
	az = { R.m[2][0], R.m[2][1], R.m[2][2] };
	ax = K4E::Vector3::Normalize(ax);
	ay = K4E::Vector3::Normalize(ay);
	az = K4E::Vector3::Normalize(az);
}

void PhysicalScene::Initialize()
{
	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	camera = K4E::CameraManager::GetInstance()->GetMainCamera();
	camera->SetTranslate({ 0.0f, 2.0f, -20.0f });
	camera->SetRotate({ 0.0f, 0.0f, 0.0f });

	// -------------------------
	// ★ OBB Rig 初期化（ざっくり人型）
	// pivot = 関節位置、pivotToCenterLocal = 関節から当たりの中心まで
	// -------------------------
	rig_.resize(NodeCount);

	// Body（root）
	rig_[Body].parent = -1;
	rig_[Body].localPivot = { 0,0,0 };                 // rootは未使用
	rig_[Body].localRotRad = { 0,0,0 };
	rig_[Body].pivotToCenterLocal = { 0.0f, 0.6f, 0.0f };
	rig_[Body].halfSize = { 0.45f, 0.60f, 0.25f };
	rig_[Body].color = { 0.0f, 1.0f, 1.0f, 1.0f };

	// Head（首位置をpivotに）
	rig_[Head].parent = Body;
	rig_[Head].localPivot = { 0.0f, 1.2f, 0.0f };      // body pivotから首へ
	rig_[Head].localRotRad = { 0,0,0 };
	rig_[Head].pivotToCenterLocal = { 0.0f, 0.25f, 0.0f };
	rig_[Head].halfSize = { 0.25f, 0.25f, 0.25f };
	rig_[Head].color = { 1.0f, 0.7f, 0.2f, 1.0f };

	// Arms（肩をpivotに）
	rig_[LeftArm].parent = Body;
	rig_[LeftArm].localPivot = { -0.55f, 1.05f, 0.0f }; // 左肩
	rig_[LeftArm].localRotRad = { 0,0,0 };
	rig_[LeftArm].pivotToCenterLocal = { 0.0f, -0.45f, 0.0f };
	rig_[LeftArm].halfSize = { 0.18f, 0.45f, 0.18f };
	rig_[LeftArm].color = { 0.3f, 1.0f, 0.3f, 1.0f };

	rig_[RightArm].parent = Body;
	rig_[RightArm].localPivot = { +0.55f, 1.05f, 0.0f }; // 右肩
	rig_[RightArm].localRotRad = { 0,0,0 };
	rig_[RightArm].pivotToCenterLocal = { 0.0f, -0.45f, 0.0f };
	rig_[RightArm].halfSize = { 0.18f, 0.45f, 0.18f };
	rig_[RightArm].color = { 0.3f, 1.0f, 0.3f, 1.0f };

	// Legs（股関節をpivotに）
	rig_[LeftLeg].parent = Body;
	rig_[LeftLeg].localPivot = { -0.25f, 0.2f, 0.0f };
	rig_[LeftLeg].localRotRad = { 0,0,0 };
	rig_[LeftLeg].pivotToCenterLocal = { 0.0f, -0.60f, 0.0f };
	rig_[LeftLeg].halfSize = { 0.20f, 0.60f, 0.20f };
	rig_[LeftLeg].color = { 0.5f, 0.6f, 1.0f, 1.0f };

	rig_[RightLeg].parent = Body;
	rig_[RightLeg].localPivot = { +0.25f, 0.2f, 0.0f };
	rig_[RightLeg].localRotRad = { 0,0,0 };
	rig_[RightLeg].pivotToCenterLocal = { 0.0f, -0.60f, 0.0f };
	rig_[RightLeg].halfSize = { 0.20f, 0.60f, 0.20f };
	rig_[RightLeg].color = { 0.5f, 0.6f, 1.0f, 1.0f };
}

void PhysicalScene::Update()
{
	const float dt = K4E::GameTimer::GetInstance()->GetDeltaTime();
	UpdateObbRig(dt);
}

void PhysicalScene::UpdateObbRig(float dt)
{
	if (!rigEnabled_) return;

	rigTime_ += dt;

	// デモ：体Yaw、腕・脚を振る
	if (rigAutoAnim_)
	{
		rig_[Body].localRotRad.y += bodyYawSpeed_ * dt;

		const float s = std::sinf(rigTime_);
		rig_[LeftArm].localRotRad.x = +armSwingAmp_ * s;
		rig_[RightArm].localRotRad.x = -armSwingAmp_ * s;

		rig_[LeftLeg].localRotRad.x = -legSwingAmp_ * s;
		rig_[RightLeg].localRotRad.x = +legSwingAmp_ * s;
	}

	// ルート
	{
		auto& n = rig_[Body];
		n.worldPivot = rigRootPivotWorld_;

		const K4E::Matrix4x4 Rlocal = K4E::Matrix4x4::MakeRotateMatrix(n.localRotRad);
		n.worldR = Rlocal;

		K4E::Vector3 ax, ay, az;
		ExtractAxes_Row(n.worldR, ax, ay, az);

		const K4E::Vector3 center =
			n.worldPivot +
			ax * n.pivotToCenterLocal.x +
			ay * n.pivotToCenterLocal.y +
			az * n.pivotToCenterLocal.z;

		n.obb.center = center;
		n.obb.size = n.halfSize;
		n.obb.orientations[0] = ax;
		n.obb.orientations[1] = ay;
		n.obb.orientations[2] = az;
	}

	// 子（親→子の順で計算）
	for (int i = 1; i < (int)rig_.size(); ++i)
	{
		auto& n = rig_[i];
		if (!n.enabled) continue;

		const auto& p = rig_[n.parent];

		// pivotのワールド位置：親pivot + (localPivot を親回転で回す)
		n.worldPivot = p.worldPivot + RotateVec_Row(n.localPivot, p.worldR);

		// 回転合成（row-vector）：Rworld = Rlocal * Rparent
		const K4E::Matrix4x4 Rlocal = K4E::Matrix4x4::MakeRotateMatrix(n.localRotRad);
		n.worldR = K4E::Matrix4x4::Multiply(Rlocal, p.worldR);

		K4E::Vector3 ax, ay, az;
		ExtractAxes_Row(n.worldR, ax, ay, az);

		// center：pivot基準の回転
		const K4E::Vector3 center =
			n.worldPivot +
			ax * n.pivotToCenterLocal.x +
			ay * n.pivotToCenterLocal.y +
			az * n.pivotToCenterLocal.z;

		n.obb.center = center;
		n.obb.size = n.halfSize;
		n.obb.orientations[0] = ax;
		n.obb.orientations[1] = ay;
		n.obb.orientations[2] = az;
	}
}

void PhysicalScene::Draw3DObjects()
{
	DrawObbRig();
}

void PhysicalScene::DrawShadowObjects()
{
}

void PhysicalScene::DrawObbRig()
{
	if (!rigEnabled_) return;

	auto* wf = K4E::Wireframe::GetInstance();
	if (!wf) return;

	// OBB描画 + pivot可視化
	for (int i = 0; i < (int)rig_.size(); ++i)
	{
		auto& n = rig_[i];
		if (!n.enabled) continue;

		wf->DrawOBB(n.obb, n.color);

		// pivot cross
		const float c = 0.12f;
		wf->DrawLine(n.worldPivot + K4E::Vector3(c, 0, 0), n.worldPivot - K4E::Vector3(c, 0, 0), K4E::Vector4(1, 1, 0, 1));
		wf->DrawLine(n.worldPivot + K4E::Vector3(0, c, 0), n.worldPivot - K4E::Vector3(0, c, 0), K4E::Vector4(1, 1, 0, 1));
		wf->DrawLine(n.worldPivot + K4E::Vector3(0, 0, c), n.worldPivot - K4E::Vector3(0, 0, c), K4E::Vector4(1, 1, 0, 1));

		// pivot->center line
		wf->DrawLine(n.worldPivot, n.obb.center, K4E::Vector4(1, 1, 1, 1));

		if (rigDrawAxes_)
		{
			const float L = rigAxisLen_;
			wf->DrawLine(n.worldPivot, n.worldPivot + n.obb.orientations[0] * L, K4E::Vector4(1, 0, 0, 1)); // X
			wf->DrawLine(n.worldPivot, n.worldPivot + n.obb.orientations[1] * L, K4E::Vector4(0, 1, 0, 1)); // Y
			wf->DrawLine(n.worldPivot, n.worldPivot + n.obb.orientations[2] * L, K4E::Vector4(0, 0, 1, 1)); // Z
		}

		// 親子の骨ライン
		if (rigDrawBones_ && n.parent >= 0)
		{
			const auto& p = rig_[n.parent];
			wf->DrawLine(p.worldPivot, n.worldPivot, K4E::Vector4(0.9f, 0.9f, 0.9f, 1));
		}
	}
}

void PhysicalScene::Draw2DSprites()
{
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
}

void PhysicalScene::Finalize()
{
}

void PhysicalScene::DrawImGui()
{
#ifdef USE_IMGUI
	camera->DrawImGui();

	ImGui::Separator();
	if (ImGui::CollapsingHeader("OBB Rig (Parent-Child)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("RigEnabled", &rigEnabled_);
		ImGui::Checkbox("AutoAnim", &rigAutoAnim_);
		ImGui::Checkbox("DrawBones", &rigDrawBones_);
		ImGui::Checkbox("DrawAxes", &rigDrawAxes_);
		ImGui::DragFloat("AxisLen", &rigAxisLen_, 0.01f, 0.1f, 5.0f);
		ImGui::DragFloat3("RootPivotWorld", &rigRootPivotWorld_.x, 0.01f);

		ImGui::DragFloat("BodyYawSpeed(rad/s)", &bodyYawSpeed_, 0.01f, -5.0f, 5.0f);
		ImGui::DragFloat("ArmSwingAmp(rad)", &armSwingAmp_, 0.01f, 0.0f, 3.14f);
		ImGui::DragFloat("LegSwingAmp(rad)", &legSwingAmp_, 0.01f, 0.0f, 3.14f);

		static const char* names[] = { "Body","Head","LeftArm","RightArm","LeftLeg","RightLeg" };
		ImGui::Combo("Node", &rigSelected_, names, IM_ARRAYSIZE(names));

		auto& n = rig_[rigSelected_];
		ImGui::Checkbox("Enabled", &n.enabled);
		ImGui::DragFloat3("LocalPivot(parent space)", &n.localPivot.x, 0.01f);
		ImGui::DragFloat3("LocalRot(rad)", &n.localRotRad.x, 0.01f);
		ImGui::DragFloat3("PivotToCenterLocal", &n.pivotToCenterLocal.x, 0.01f);
		ImGui::DragFloat3("HalfSize", &n.halfSize.x, 0.01f, 0.01f, 5.0f);
	}
#endif
}