#pragma once
#include <BaseScene.h>

#include <Vector4.h>
#include <Vector3.h>
#include <Matrix4x4.h>
#include <OBB.h>

#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }
namespace Ken4lowEngine { class Camera; }

/// -------------------------------------------------------------
//				物理シーン（デバッグテスト用・サブシーン）
/// -------------------------------------------------------------
class PhysicalScene : public BaseScene
{
public:
	void Initialize() override;
	void Update() override;
	void Draw3DObjects() override;
	void Draw2DSprites() override;
	void Finalize() override;
	void DrawImGui() override;

private:
	// ★ OBB親子リグの更新/描画
	void UpdateObbRig(float dt);
	void DrawObbRig();

private:
	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;
	K4E::Camera* camera = nullptr;

private:
	// -------------------------
	// ★ OBB Rig (Head/Body/Limbs)
	// -------------------------
	struct ObbRigNode
	{
		int parent = -1;                    // 親ノードindex（-1:root）
		bool enabled = true;

		K4E::Vector3 localPivot{};          // 親pivotから見た、このpivotのローカル位置（parent空間）
		K4E::Vector3 localRotRad{};         // pivot周り回転（ラジアン）
		K4E::Vector3 pivotToCenterLocal{};  // pivot→OBB中心（nodeローカル）
		K4E::Vector3 halfSize{ 0.3f,0.3f,0.3f };

		K4E::Vector4 color{ 0,1,1,1 };

		// 計算結果
		K4E::Vector3 worldPivot{};
		K4E::Matrix4x4 worldR{};            // 回転のみ（row-vector前提）
		K4E::OBB obb{};
	};

	enum NodeId
	{
		Body = 0,
		Head,
		LeftArm,
		RightArm,
		LeftLeg,
		RightLeg,
		NodeCount
	};

	std::vector<ObbRigNode> rig_;

	// リグのルート（体）のワールドpivot
	K4E::Vector3 rigRootPivotWorld_{ 0.0f, 1.0f, 0.0f };

	// 表示/挙動
	bool rigEnabled_ = true;
	bool rigDrawBones_ = true;       // 親pivot↔子pivotを線で描く
	bool rigDrawAxes_ = true;        // pivotの軸表示
	float rigAxisLen_ = 0.6f;

	// デモ用アニメ
	bool rigAutoAnim_ = true;
	float rigTime_ = 0.0f;
	float bodyYawSpeed_ = 1.0f;      // rad/sec
	float armSwingAmp_ = 0.8f;       // rad
	float legSwingAmp_ = 0.6f;       // rad

	// ImGui用
	int rigSelected_ = 0;
};