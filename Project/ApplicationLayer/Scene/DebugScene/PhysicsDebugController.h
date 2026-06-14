#pragma once
#include "Collider.h"
#include "Engine/Physics/Event/IPhysicsEventListener.h"
#include "Engine/Physics/Bridge/StagePhysicsBinder.h"
#include "Engine/Physics/Bridge/PhysicsParameterBridge.h"
#include "Engine/Physics/Debug/PhysicsDebugDraw.h"
#include "PhysicsWorld.h"
#include "Rigidbody.h"
#include "Vector3.h"

#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					DebugScene用 物理確認コントローラ
/// -------------------------------------------------------------
class PhysicsDebugController : public K4E::IPhysicsEventListener
{
public: /// ---------- メンバ関数 ---------- ///

	// 破棄時にPhysicsWorldからイベントリスナー登録を解除する。
	~PhysicsDebugController() override;

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

	// PhysicsWorldから届いたイベントをDebugScene上の確認用状態へ反映する。
	void OnPhysicsEvent(const K4E::PhysicsEvent& event) override;

private: /// ---------- メンバ関数 ---------- ///

	// テスト用Colliderへ現在の位置と形状を同期する。
	void UpdateTestColliders();

	// DebugScene上で確認するCollisionLayer応答をPhysicsWorldへ適用する。
	void ApplyResponseSetting();

	// DebugScene上で登録確認に使う仮Stage Collider群を初期化する。
	void InitializeDebugStageColliders();

	// イベント反応確認用の状態とログを初期化する。
	void ClearEventReactionState();

	// 最新イベントログを保存し、表示件数が増えすぎないように制限する。
	void AddEventLog(const K4E::PhysicsEvent& event);

	// ParameterManagerから読み込んだ物理設定をDebugScene用Worldへ反映する。
	void ApplyParameterSettings();

private: /// ---------- メンバ変数 ---------- ///

	K4E::PhysicsWorld physicsWorld_; // DebugScene専用の物理ワールド
	K4E::Rigidbody dynamicRigidbody_; // Dynamic側のテスト剛体
	K4E::Rigidbody staticRigidbody_; // Static側のテスト剛体

	K4E::Collider dynamicCollider_; // Dynamic側のテストCollider
	K4E::Collider staticCollider_; // Static側のテストCollider
	std::vector<K4E::Collider> debugStageColliders_{}; // Binder確認用の仮Stage Collider群
	std::vector<K4E::Collider*> debugStageColliderPointers_{}; // 仮Stage Colliderの参照ポインタ一覧

	K4E::Vector3 dynamicPosition_{}; // Dynamic側の現在位置
	K4E::Vector3 staticPosition_{}; // Static側の現在位置
	K4E::Vector3 dynamicInitialPosition_{ 0.0f, 4.0f, 0.0f }; // Dynamic側の初期位置
	K4E::Vector3 staticInitialPosition_{ 0.0f, -0.5f, 0.0f }; // Static側の初期位置
	K4E::Vector3 dynamicInitialVelocity_{ 3.0f, 0.0f, 0.0f }; // Dynamic側の初期速度
	K4E::Vector3 dynamicHalfSize_{ 0.75f, 0.75f, 0.75f }; // Dynamic側のAABB半サイズ
	K4E::Vector3 staticHalfSize_{ 5.0f, 0.5f, 5.0f }; // Static側のAABB半サイズ

	bool enablePhysicsStep_ = true; // PhysicsWorld::Stepを進めるか
	bool enableResolve_ = true; // Contactによる位置/速度補正を行うか
	bool enableVelocityResolve_ = true; // Contactによる速度補正を行うか
	bool enableFriction_ = true; // Contactによる摩擦補正を行うか
	bool enableSleep_ = true; // Sleep状態への移行を有効にするか
	bool useGravity_ = true; // Dynamic側へ重力を適用するか
	float mass_ = 1.0f; // Dynamic側の質量
	float restitution_ = 0.0f; // Dynamic側の反発係数
	float staticFriction_ = 0.5f; // Dynamic側の静止摩擦係数
	float dynamicFriction_ = 0.2f; // Dynamic側の動摩擦係数
	float sleepSpeedThreshold_ = 0.05f; // Sleep判定用の速度閾値
	float sleepTimeThreshold_ = 0.5f; // Sleep判定用の時間閾値
	float initialHorizontalSpeed_ = 3.0f; // Reset時に与える横方向初速
	int dynamicLayer_ = 1; // Dynamic側ColliderのCollisionLayer
	int staticLayer_ = 0; // Static側ColliderのCollisionLayer
	int responseTypeIndex_ = 2; // Debug確認用Response選択。0:Ignore, 1:Trigger, 2:Block
	bool showStagePhysicsColliders_ = true; // Binder確認用Stage Colliderをワイヤー表示するか
	K4E::StagePhysicsBinder stagePhysicsBinder_{}; // Stage Collider群をPhysicsWorldへ登録する橋渡し確認用
	K4E::PhysicsParameterBridge physicsParameterBridge_{}; // ParameterManagerとPhysicsWorldの橋渡し
	K4E::PhysicsDebugDraw physicsDebugDraw_{}; // PhysicsWorld全体の共通Debug可視化

	bool isTriggerTouching_ = false; // Trigger接触中か
	bool isCollisionTouching_ = false; // Block接触中か
	int triggerEnterCount_ = 0; // TriggerEnter通知回数
	int triggerStayCount_ = 0; // TriggerStay通知回数
	int triggerExitCount_ = 0; // TriggerExit通知回数
	int collisionEnterCount_ = 0; // CollisionEnter通知回数
	int collisionStayCount_ = 0; // CollisionStay通知回数
	int collisionExitCount_ = 0; // CollisionExit通知回数
	std::vector<std::string> eventLogs_{}; // Debug表示用の最新イベントログ
};
