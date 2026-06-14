#pragma once
#include "Collider.h"
#include "PhysicsWorld.h"
#include "Rigidbody.h"
#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					DebugScene用 物理確認コントローラ
/// -------------------------------------------------------------
class PhysicsDebugController
{
public: /// ---------- メンバ関数 ---------- ///

	// DebugScene専用の物理テストを初期化する。
	void Initialize();

	// DebugScene専用の物理確認処理を更新する。
	void Update(float deltaTime);

	// Debug用の物理テスト形状を描画する。
	void Draw();

	// 物理テスト用の調整・確認UIを描画する。
	void DrawImGui();

	// テスト用のDynamic/Staticオブジェクトを初期状態に戻す。
	void ResetTestObjects();

private: /// ---------- メンバ関数 ---------- ///

	// テスト用Colliderへ現在の位置と形状を同期する。
	void UpdateTestColliders();

private: /// ---------- メンバ変数 ---------- ///

	K4E::PhysicsWorld physicsWorld_; // DebugScene専用の物理ワールド
	K4E::Rigidbody dynamicRigidbody_; // Dynamic側のテスト剛体
	K4E::Rigidbody staticRigidbody_; // Static側のテスト剛体

	K4E::Collider dynamicCollider_; // Dynamic側のテストCollider
	K4E::Collider staticCollider_; // Static側のテストCollider

	K4E::Vector3 dynamicPosition_{}; // Dynamic側の現在位置
	K4E::Vector3 staticPosition_{}; // Static側の現在位置
	K4E::Vector3 dynamicInitialPosition_{ 0.0f, 4.0f, 0.0f }; // Dynamic側の初期位置
	K4E::Vector3 staticInitialPosition_{ 0.0f, -0.5f, 0.0f }; // Static側の初期位置
	K4E::Vector3 dynamicInitialVelocity_{ 0.0f, 0.0f, 0.0f }; // Dynamic側の初期速度
	K4E::Vector3 dynamicHalfSize_{ 0.75f, 0.75f, 0.75f }; // Dynamic側のAABB半サイズ
	K4E::Vector3 staticHalfSize_{ 5.0f, 0.5f, 5.0f }; // Static側のAABB半サイズ

	bool enablePhysicsStep_ = true; // PhysicsWorld::Stepを進めるか
	bool enableResolve_ = true; // Contactによる位置/速度補正を行うか
	bool useGravity_ = true; // Dynamic側へ重力を適用するか
	float mass_ = 1.0f; // Dynamic側の質量
	float restitution_ = 0.0f; // Dynamic側の反発係数
};
