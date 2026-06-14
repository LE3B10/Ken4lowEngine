#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "Collider.h"
#include "PhysicsWorld.h"
#include "Rigidbody.h"
#include "Vector3.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine
{
	class DirectXCommon;
	class Input;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	~DebugScene() override = default;

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;

	// 仮想3D描画処理
	void Draw3DObjects() override;

	// 仮想シャドウマップ描画処理
	void DrawShadowObjects() override;

	// 仮想2D描画処理
	void Draw2DSprites() override;

	// 仮想終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// デバッグカメラの更新
	void UpdateDebug();

	// PhysicsWorld単体確認用の初期状態へ戻す
	void ResetPhysicsDebugTest();

	// PhysicsWorld単体確認用の更新
	void UpdatePhysicsDebugTest(float deltaTime);

	// PhysicsWorld単体確認用のImGuiを描画する
	void DrawPhysicsDebugImGui();

	// PhysicsWorld単体確認用Colliderの位置と形状を更新する
	void UpdatePhysicsDebugColliders();

private: /// ---------- メンバ変数 ---------- ///

	K4E::DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのポインタ
	K4E::Input* input_ = nullptr; // Inputのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突管理マネージャー

	K4E::PhysicsWorld physicsWorld_; // DebugScene専用の物理ワールド
	K4E::Rigidbody physicsTestRigidbody_; // DebugScene専用のテスト剛体
	K4E::Rigidbody physicsStaticRigidbody_; // Contact確認用の静的剛体
	K4E::Vector3 physicsTestPosition_{}; // DebugScene専用のテスト位置
	K4E::Vector3 physicsTestInitialPosition_{ 0.0f, 4.0f, 0.0f }; // テスト初期位置
	K4E::Vector3 physicsTestInitialVelocity_{ 0.0f, 0.0f, 0.0f }; // テスト初期速度
	bool physicsTestUseGravity_ = true; // テスト用重力フラグ
	float physicsTestMass_ = 1.0f; // テスト用質量
	float physicsTestRestitution_ = 0.0f; // テスト用反発係数

	K4E::Collider physicsStaticCollider_; // Contact確認用の静的Collider
	K4E::Collider physicsDynamicCollider_; // Contact確認用の動的Collider
	K4E::Vector3 physicsStaticColliderPosition_{ 0.0f, -0.5f, 0.0f }; // 静的Collider位置
	K4E::Vector3 physicsDynamicColliderHalfSize_{ 0.75f, 0.75f, 0.75f }; // 動的Collider半サイズ
	K4E::Vector3 physicsStaticColliderHalfSize_{ 5.0f, 0.5f, 5.0f }; // 静的Collider半サイズ
	bool physicsPositionSolveEnabled_ = true; // Contactによる位置/速度補正の有効フラグ
};
