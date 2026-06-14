#pragma once
#include "Contact.h"

#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	class Rigidbody;

	/// -------------------------------------------------------------
	///                         物理ワールド
	/// -------------------------------------------------------------
	class PhysicsWorld
	{
	public: /// ---------- メンバ関数 ---------- ///

		// Colliderを物理ワールドへ登録する。
		void RegisterCollider(Collider* collider);

		// Colliderを物理ワールドから登録解除する。
		void UnregisterCollider(Collider* collider);

		// Rigidbodyを物理ワールドへ登録する。
		void RegisterRigidbody(Rigidbody* rigidbody);

		// Rigidbodyを物理ワールドから登録解除する。
		void UnregisterRigidbody(Rigidbody* rigidbody);

		// 1ステップ分の物理更新を実行する。
		void Step(float deltaTime);

		// 固定更新用の入口として物理更新を進める。
		void Update(float deltaTime);

		// 登録済みRigidbodyの速度積分を行う。
		void IntegrateBodies(float deltaTime);

		// 登録済みCollider同士の接触を検出する。
		void DetectCollisions();

		// 検出済みContactを解決する。
		void ResolveContacts();

		// 直近ステップのContact一覧を取得する。
		const std::vector<Contact>& GetContacts() const { return contacts_; }

		// 位置補正ソルバーの有効状態を設定する。
		void SetPositionSolveEnabled(bool enabled) { positionSolveEnabled_ = enabled; }
		bool IsPositionSolveEnabled() const { return positionSolveEnabled_; }

		// 摩擦ソルバーの有効状態を設定する。
		void SetFrictionSolveEnabled(bool enabled) { frictionSolveEnabled_ = enabled; }
		bool IsFrictionSolveEnabled() const { return frictionSolveEnabled_; }

		// 固定更新の有効状態を設定する。
		void SetUseFixedStep(bool useFixedStep);
		bool IsUseFixedStep() const { return useFixedStep_; }

		// 固定更新の1ステップ時間を設定する。
		void SetFixedTimeStep(float fixedTimeStep);
		float GetFixedTimeStep() const { return fixedTimeStep_; }

		// 1フレームで受け付ける最大deltaTimeを設定する。
		void SetMaxDeltaTime(float maxDeltaTime);
		float GetMaxDeltaTime() const { return maxDeltaTime_; }

		// 1フレームで実行する最大サブステップ数を設定する。
		void SetMaxSubSteps(int maxSubSteps);
		int GetMaxSubSteps() const { return maxSubSteps_; }

		float GetAccumulator() const { return accumulator_; }
		int GetLastSubStepCount() const { return lastSubStepCount_; }

	private: /// ---------- メンバ関数 ---------- ///

		// Rigidbodyの接地などのフレーム状態をリセットする。
		void ClearRigidbodyFrameState();

		// RigidbodyのSleep状態を更新する。
		void UpdateRigidbodySleepState(float deltaTime);

		// Contact normalから床接触状態を更新する。
		void UpdateGroundedState(const Contact& contact) const;

		// Colliderペアが接触しているかを既存Primitive判定で調べる。
		bool TestCollisionPair(Collider* colliderA, Collider* colliderB) const;

		// ColliderペアからContactを構築する。
		Contact BuildContact(Collider* colliderA, Collider* colliderB) const;

	private: /// ---------- メンバ変数 ---------- ///

		// 物理ワールドが参照するCollider一覧。所有権は持たない。
		std::vector<Collider*> colliders_{};

		// 物理ワールドが参照するRigidbody一覧。所有権は持たない。
		std::vector<Rigidbody*> rigidbodies_{};

		// 直近ステップで検出されたContact一覧。
		std::vector<Contact> contacts_{};

		// Contact解決で位置補正を行うか。DebugSceneからON/OFFを切り替える。
		bool positionSolveEnabled_ = true;

		// Contact解決で摩擦補正を行うか。DebugSceneからON/OFFを切り替える。
		bool frictionSolveEnabled_ = true;

		// 固定更新とサブステップ制御用の状態。
		bool useFixedStep_ = true;
		float fixedTimeStep_ = 1.0f / 60.0f;
		float maxDeltaTime_ = 0.1f;
		float accumulator_ = 0.0f;
		int maxSubSteps_ = 4;
		int lastSubStepCount_ = 0;
	};

} // namespace Ken4lowEngine
