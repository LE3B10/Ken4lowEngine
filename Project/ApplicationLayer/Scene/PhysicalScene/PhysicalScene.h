#pragma once
#include <BaseScene.h>

#include <AABB.h>
#include <Capsule.h>
#include <OBB.h>
#include <Plane.h>
#include <Segment.h>
#include <Sphere.h>
#include <Triangle.h>

#include <vector>

/// ---------- 前方宣言 ---------- ///
class Input;

/// -------------------------------------------------------------
//				　		物理シーン
/// -------------------------------------------------------------
class PhysicalScene : public BaseScene
{
	// コライダータイプ
	enum class ColliderType
	{
		None, // コライダーなし
		Ground, // 地面の平面
		Wall, // 壁のAABB
	};


public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力管理クラス

	AABB aabb_ = {};
	AABB wallAABB_ = {}; // 壁用のAABBを追加


	Capsule capsule_ = {};
	OBB obb_ = {};
	Plane plane_ = {};
	Segment segment_ = {};
	Sphere sphere_ = {};
	Triangle triangle_ = {};

	float gravity_ = 0.01f; // 重力の値
	bool isGrounded_ = false;     // 接地判定
	float jumpVelocity_ = 0.0f;   // ジャンプの上向き速度
	const float jumpPower_ = 0.25f; // ジャンプ初速度

	// 壁判定
	bool isWallCollision_ = false;

	std::vector<std::pair<ColliderType, AABB>> aabbs_;
};

