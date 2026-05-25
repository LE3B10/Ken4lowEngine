#pragma once

#include "EnemyBase.h"
#include "../AI/MeleeAttackController.h"
#include "../Navigation/EnemyAStarNavigator.h"
#include <string>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					　近接攻撃型敵クラス
/// -------------------------------------------------------------
class MeleeEnemy final : public EnemyBase
{
private: /// ---------- 列挙型 ---------- ///

	// 近接攻撃の種類
	enum class AnimState
	{
		Idle,	 // 待機
		Walk,	 // 歩行
		Scratch, // 引っ掻き
		OneTwo,	 // 連続攻撃
		Dead,	 // 死亡
	};

private: /// ---------- 構造体 ---------- ///

	// プレイヤー検知と攻撃の距離設定
	struct DetectionSettings
	{
		float detectRange = 18.0f;			   // プレイヤーを検知する距離
		float meleeAttackRange = 2.8f;		   // 近接攻撃を開始する距離
		float stopDistance = 1.8f;			   // プレイヤーに近づきすぎない距離
		float attackStartRange = 2.4f;		   // 攻撃開始と歩行の切り替え距離
		float resumeChaseDistance = 2.8f;	   // 攻撃後に追いかけを再開する距離
		float minOneTwoForwardDistance = 1.6f; // OneTwo攻撃の前進距離が有効になる最小距離
	};

	// 移動と回転の設定
	struct MoveSettings
	{
		float moveSpeed = 3.2f;					 // 基本移動速度
		float rotateSpeed = 8.0f;				 // 回転速度
		float maxResolvePushPerFrame = 0.75f;	 // ステージ衝突解決の最大押し戻し距離
		float maxHorizontalPushPerFrame = 0.45f; // 水平方向の衝突解決の最大押し戻し距離
		bool obstacleTopLandingEnabled = true;	 // 障害物上面への着地判定を有効にする
		float obstacleTopLandingTolerance = 0.35f; // 障害物上面への着地を許容する足元の高さ誤差
		float obstacleTopLandingMaxHeight = 3.5f; // 障害物上面へ着地可能な最大段差
		float obstacleTopLandingMinHorizontalOverlap = 0.2f; // 上面着地に必要なXZ最小重なり量
	};

	// 落下と重力の設定
	struct JumpSettings
	{
		bool enabled = true;				// ジャンプを有効にするかどうか
		float baseVelocity = 12.0f;			// ジャンプの基本垂直速度
		float extraBoost = 0.8f;			// 攻撃の種類や状況に応じた追加の垂直速度の倍率
		float gravityEstimate = 20.0f;		// ジャンプの軌道計算に使用する重力の推定値（実際の重力とは異なる場合がある）
		float maxVelocity = 18.0f;			// ジャンプの最大垂直速度
		float targetHeightThreshold = 1.0f;	// ターゲットの高さとの差がこの値以上ある場合にジャンプを試みる
		float horizontalDistanceMax = 8.0f;	// ジャンプを試みる最大水平距離
		float cooldown = 0.9f;				// ジャンプのクールダウン時間
	};

	// ジャンプの状態管理
	struct JumpState
	{
		float cooldownTimer = 0.0f;		 // ジャンプのクールダウンタイマー
		float targetHeightDelta = 0.0f;	 // ジャンプ開始時のターゲットの高さとの差
		float calculatedVelocity = 0.0f; // ジャンプの軌道計算から得られる垂直速度
		float appliedVelocity = 0.0f;	 // 実際に適用された垂直速度
		std::string lastReason = "None"; // ジャンプを試みた最後の理由（デバッグ用）
	};

	// 経路探索の設定
	struct PathSettings
	{
		bool enabled = true;					// 経路探索を有効にするかどうか
		float repathInterval = 0.25f;			// 定期的にリパスする間隔
		float waypointReachDistance = 0.85f;	// ウェイポイントを「到達した」とみなす距離
		float gridSize = 1.5f;					// 経路探索のグリッドサイズ
		float searchRadius = 28.0f;				// 経路探索のための周囲の検索半径
		float obstacleExpandRadius = 0.9f;		// 経路探索時に障害物を拡張する半径
		float temporaryBlockDuration = 1.5f;	// 一時的にブロックする障害物の持続時間
		float temporaryBlockRadius = 1.0f;		// 一時的にブロックする障害物の半径
		bool cornerCuttingDisabled = true;		// 経路の角を切り落とす（斜め移動を許可する）かどうか
		float targetRepathThreshold = 1.2f;		// ターゲットがこの距離以上動いたらリパスする
		float stuckRepathExpandBonus = 0.25f;	// スタックリパス時の拡張ボーナス
		float maxStuckRepathExpandBonus = 1.0f; // スタックリパス時の最大拡張ボーナス
	};

	// 経路探索の状態管理
	struct PathState
	{
		bool found = false;						   // 経路が見つかったかどうか
		std::string failureReason = "None";		   // 経路探索に失敗した理由（デバッグ用）
		float failedWaitTimer = 0.0f;			   // 経路探索に失敗してからの待機時間
		float lastRepathTimer = 0.0f;			   // 最後のリパスからの経過時間
		float retryTimer = 0.0f;				   // 経路探索に失敗した後のリトライタイマー
		float targetMovedDistanceForRepath = 0.0f; // ターゲットが動いた距離（リパス判定用）
		float lastMovedDistance = 0.0f;			   // 最後の位置からの移動距離（スタック判定用）
		std::string lastRepathReason = "None";	   // 最後のリパス理由（デバッグ用）
		std::string blockedObstacleName = "None";  // 経路がブロックされたときの障害物の名前（デバッグ用）
		K4E::Vector3 lastStuckCheckPosition{};	   // 最後のスタックチェック位置
		K4E::Vector3 lastPathTargetPos{};		   // 最後に経路探索したときのターゲット位置
		K4E::Vector3 currentWaypoint{};			   // 現在のウェイポイント
		K4E::Vector3 blockedSegmentFrom{};		   // 経路がブロックされたときのセグメントの開始点（デバッグ用）
		K4E::Vector3 blockedSegmentTo{};		   // 経路がブロックされたときのセグメントの終了点（デバッグ用）
		int blockedWaypointIndex = -1;			   // 経路がブロックされたときのウェイポイントのインデックス（デバッグ用）
		bool lineBlocked = false;				   // ターゲットへの直線が障害物でブロックされているかどうか
	};

	// スタックの設定
	struct StuckSettings
	{
		float checkTime = 0.8f;		 // スタックしているかどうかを判定するための時間
		float distance = 0.2f;		 // スタックとみなす距離
		float moveThreshold = 0.18f; // スタックとみなす移動距離の閾値
	};

	// スタックの状態管理
	struct StuckState
	{
		bool isStuck = false; // スタックしているかどうか
		float timer = 0.0f;	  // スタックしていると判断してからの経過時間
	};

	// 攻撃の設定
	struct AttackSettings
	{
		MeleeAttackType selectedAttackType = MeleeAttackType::Scratch; // 現在選択されている攻撃の種類
		float lockTime = 0.18f;										   // 攻撃開始後に向きをロックする時間
	};

	// 攻撃の状態管理
	struct AttackState
	{
		float lockTimer = 0.0f;	 // 攻撃開始後の向きロックタイマー
		bool shouldChase = true; // 攻撃中に追いかけるかどうか
	};

	// アニメーションの設定
	struct AnimationSettings
	{
		float visualYawOffset = 0.0f;	 // 見た目の向きのオフセット（攻撃中などに体を傾けるため）
		float walkAnimSpeed = 8.0f;		 // 歩行アニメーションの速度
		float walkArmSwing = 0.55f;		 // 歩行中の腕の振り幅
		float walkLegSwing = 0.45f;		 // 歩行中の脚の振り幅
		float attackArmSwing = 1.25f;	 // 攻撃中の腕の振り幅
		float attackReturnSpeed = 12.0f; // 攻撃後に腕を戻す速度
		float attackBodyLean = 0.15f;	 // 攻撃中の体の傾きの強さ
	};

	// アニメーションの状態管理
	struct AnimationStateData
	{
		float walkAnimTime = 0.0f;							// 歩行アニメーションの時間
		float rawYaw = 0.0f;								// 目標に向かうための生の向き（移動方向やターゲット方向）
		float finalVisualYaw = 0.0f;						// 実際に見た目を回す向き（visualYawOffset を加味したもの）
		float visualYawOffsetDeg = 0.0f;					// 見た目の向きのオフセット（度単位、デバッグ用）
		float debugCurrentYaw = 0.0f;						// デバッグ用：現在の向き（度単位）
		float debugTargetYaw = 0.0f;						// デバッグ用：目標の向き（度単位）
		float debugDeltaYaw = 0.0f;							// デバッグ用：向きの差（度単位）
		float debugNormalizedDeltaYaw = 0.0f;				// デバッグ用：向きの差を -180～180 に正規化したもの（度単位）
		K4E::Vector3 facingDirection{ 0.0f, 0.0f, 1.0f };	// 向いている方向（正規化されたベクトル）
		K4E::Vector3 movementDirection{ 0.0f, 0.0f, 1.0f };	// 移動している方向（正規化されたベクトル）
		K4E::Vector3 targetDirection{ 0.0f, 0.0f, 1.0f };	// ターゲットの方向（正規化されたベクトル）
		K4E::Vector3 visualForward{ 0.0f, 0.0f, 1.0f };		// 見た目の前方向（visualYawOffset を加味したもの）
		K4E::Vector3 attackForward{ 0.0f, 0.0f, 1.0f };		// 攻撃の前方向（攻撃の種類や状況に応じて変わる）
		AnimState animState = AnimState::Idle;				// 現在のアニメーション状態
	};

	// 衝突の状態管理
	struct CollisionState
	{
		K4E::Vector3 lastSafePosition{};							 // 最後に安全と判断した位置（ステージ内で非衝突の位置）
		K4E::Vector3 stageBoundsMin{ -1000.0f, -1000.0f, -1000.0f }; // ステージの境界の最小点（デバッグ用、必要に応じて調整）
		K4E::Vector3 stageBoundsMax{ 1000.0f, 1000.0f, 1000.0f };	 // ステージの境界の最大点（デバッグ用、必要に応じて調整）
		bool hasStageBounds = false;								 // ステージの境界が設定されているかどうか
		bool isOutsideStage = false;								 // ステージの外にいるかどうか
		K4E::Vector3 lastResolvePush{};								 // 最後の衝突解決の押し戻しベクトル（デバッグ用）
		bool isCollidingWithStage = false;							 // ステージの衝突と接触しているかどうか
		std::string lastStageCollisionType = "None";				 // 最後に衝突したステージのタイプ（床、壁など）（デバッグ用）
		std::string lastStageCollisionName = "None";				 // 最後に衝突したステージオブジェクトの名前（デバッグ用）
		bool blockedByObstacle = false;								 // 経路探索の障害物によってブロックされているかどうか
		std::string lastBlockedObstacleName = "None";				 //	最後に経路探索の障害物によってブロックされたときの障害物の名前（デバッグ用）
		int usingWorldAABBCount = 0;								 // ステージのAABBと接触している数（デバッグ用）
		int usingObstacleAABBCount = 0;								 // 経路探索の障害物のAABBと接触している数（デバッグ用）
		bool collisionManagerRegistered = false;					 // 衝突マネージャーに登録されているかどうか
		int lastCollisionCount = 0;									 // 最後のフレームで衝突していたオブジェクトの数（デバッグ用）
		bool pushedThisFrame = false;								 // 今フレームで衝突解決の押し戻しが行われたかどうか
		bool restoredToSafePosition = false;						 // 今フレームで安全な位置への復帰が行われたかどうか
		bool isOverlappingWallObstacle = false;						 // 壁の障害物と重なっているかどうか
		bool isOnFloor = false;										 // 床の上にいるかどうか
		std::string lastWallObstacleName = "None";					 // 最後に重なった壁の障害物の名前（デバッグ用）
		K4E::Vector3 lastWallResolvePush{};							 // 壁の障害物と重なっているときの最後の衝突解決の押し戻しベクトル（デバッグ用）
		bool landedOnObstacleTop = false;								 // 障害物上面への着地が成立したかどうか
		std::string lastObstacleTopLandingName = "None";				 // 最後に上面着地した障害物名（デバッグ用）
	};

	// 徘徊の状態管理
	struct WanderState
	{
		float timer = 0.0f;							// 徘徊の行動を切り替えるためのタイマー
		K4E::Vector3 direction{ 1.0f, 0.0f, 0.0f };	// 徘徊の移動方向（正規化されたベクトル）
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突開始の処理（ターゲットとの接触やステージの衝突など）
	void OnCollisionEnter(K4E::Collider* other) override;

	// 衝突中の処理（ステージの衝突解決やスタック判定など）
	void OnCollisionStay(K4E::Collider* other) override;

public: /// ---------- アクセッサ ---------- ///

	// ターゲットとステージのAABBの設定
	void SetTarget(K4E::Collider* target) { target_ = target; }

	// ステージのAABBは外部で管理されているものへのポインタを受け取る形にする
	void SetFloorAABBs(const std::vector<K4E::AABB>* aabbs) { floorAABBs_ = aabbs; }

	// 壁の障害物のAABBも同様に外部で管理されているものへのポインタを受け取る形にする
	void SetWallObstacleAABBs(const std::vector<K4E::AABB>* aabbs) { wallObstacleAABBs_ = aabbs; }

	/// ----- ターゲットに関するアクセッサ ----- ///

	// ターゲットのコライダーを取得
	K4E::Collider* GetTargetCollider() const { return target_; }

	// ターゲットの位置を取得（ターゲットがいる場合はその位置、いない場合はゼロベクトルを返す）
	K4E::Vector3 GetTargetPositionForAttack() const;

	/// ----- アニメーションに関するアクセッサ ----- ///

	// アニメーションの状態を取得
	K4E::Vector3 GetAttackForward() const { return animationState_.attackForward; }

	// 攻撃の前方向を計算するためのアクセッサ（攻撃の種類や状況に応じて変わる）
	void ApplyAttackMove(const K4E::Vector3& horizontalVelocity);

	// 攻撃のヒットを通知するための関数（攻撃の前方向も渡す）
	void NotifyAttackHit(int damage, const K4E::Vector3& forward);

	// 現在のアニメーション状態を取得
	void ForceAttack(MeleeAttackType type);

	// 攻撃のロックタイマーを取得
	void StopAttack();

	// 攻撃のクールダウンをリセット
	void ResetAttackCooldown();

	// 現在選択されている攻撃の種類を設定・取得
	void SetSelectedAttackType(MeleeAttackType type) { attackSettings_.selectedAttackType = type; }

	// 選択されている攻撃の種類を取得
	MeleeAttackType GetSelectedAttackType() const { return attackSettings_.selectedAttackType; }

	// 攻撃中かどうかを取得
	bool IsAttacking() const { return attackController_.IsAttacking(); }

	// 現在の攻撃の名前を取得（デバッグ用）
	const char* GetCurrentAttackName() const { return attackController_.GetCurrentAttackName(); }

private: /// ---------- 内部処理 ---------- ///

	// ターゲットの検知と攻撃の判定
	bool HasTarget() const;

	// ターゲットとの距離や状況に応じた行動の判定
	bool IsTargetInDetectRange() const;

	// ターゲットが近接攻撃の範囲内にいるかどうかを判定
	bool IsTargetInMeleeRange() const;

	// ターゲットが攻撃開始と歩行の切り替え距離内にいるかどうかを判定
	bool IsTargetInAttackStartRange() const;

	// 攻撃後に追いかけを再開する距離内にターゲットがいるかどうかを判定
	bool IsTargetInAttackHoldRange() const;

	// OneTwo攻撃の前進距離が有効になる最小距離内にターゲットがいるかどうかを判定
	bool IsAttackCooldownReady() const;

	// ターゲットの高さとの差がジャンプを試みるための閾値以上あるかどうかを判定
	bool IsDeadCondition() const;

	// ターゲットとの距離を取得
	float GetDistanceToTarget() const;

	// ターゲットの方向を取得（正規化されたベクトル）
	K4E::Vector3 GetTargetPosition() const;

	// ターゲットの方向を取得（正規化されたベクトル）
	void FaceToTarget(float deltaTime);

	// 移動方向に向く処理
	void FaceToMoveDirection(float deltaTime);

	// 見た目の向きを移動方向やターゲット方向に合わせる処理（visualYawOffset を加味して回す）
	void ApplyVisualYawFromDirection(const K4E::Vector3& direction, float deltaTime);

	// 移動の処理（経路探索に従って移動する、障害物を避ける、ジャンプするなど）
	void StopMove();

	// ターゲットに向かって移動する処理（経路探索を使用して移動する）
	bool IsMoveResumeDistanceReached() const;

	// 経路探索に従って移動する処理（ウェイポイントに向かって移動し、到達したら次のウェイポイントへ）
	bool MoveAlongPath(float deltaTime);

	// 障害物を避ける処理（壁の障害物やステージの衝突を解決する）
	void TryJumpToTarget(float deltaTime);

	// ジャンプの軌道計算を行い、ターゲットの高さに合わせた垂直速度を計算する処理
	float CalculateJumpVelocityForHeight(float heightDelta) const;

	// ジャンプを試みる処理（ターゲットの高さや距離、ジャンプのクールダウンなどを考慮してジャンプするかどうかを判断し、実際にジャンプする）
	bool ResolveObstaclePenetrationXZ(float deltaTime);
	// 障害物の上面へ着地できる場合にY位置を補正して横押し出しより優先する
	bool TryLandOnObstacleTop(float deltaTime);

	// ステージの衝突を解決する処理（AABBとの衝突を解決して押し戻す）
	void UpdateStuckState(float deltaTime);

	// 攻撃の処理（攻撃の種類や状況に応じて攻撃を実行する）
	void DeadAction();

	// 攻撃の処理（攻撃の種類や状況に応じて攻撃を実行する）
	void MeleeAttackAction();

	// 攻撃の処理（攻撃の種類や状況に応じて攻撃を実行する）
	void CombatIdleAction();

	// 攻撃の処理（攻撃の種類や状況に応じて攻撃を実行する）
	void ChaseTargetAction();

	// 攻撃の処理（攻撃の種類や状況に応じて攻撃を実行する）
	void WanderAction(float deltaTime);

	// アニメーションの処理（現在の状態に応じたアニメーションの更新や見た目の向きの調整など）
	void EvaluateBehavior(float deltaTime);

	// アニメーションの処理（現在の状態に応じたアニメーションの更新や見た目の向きの調整など）
	void UpdateVisualAnimation(float deltaTime);

	// ステージの衝突を解決する処理（AABBとの衝突を解決して押し戻す）
	bool IsInsideStageBounds(const K4E::Vector3& position) const;

	// ステージの衝突を解決する処理（AABBとの衝突を解決して押し戻す）
	const char* GetAnimStateName() const;

private: /// ---------- メンバ変数 ---------- //

	// ターゲットのコライダーへのポインタ（プレイヤーなど）
	K4E::Collider* target_ = nullptr;

	// 各種設定と状態管理の構造体
	DetectionSettings detection_{};

	// 移動と回転の設定
	MoveSettings move_{};

	// 落下と重力の設定
	JumpSettings jump_{};

	// ジャンプの状態管理
	JumpState jumpState_{};

	// 経路探索の設定
	PathSettings pathSettings_{};

	// 経路探索の状態管理
	PathState pathState_{};

	// スタック状態の設定
	StuckSettings stuckSettings_{};

	// スタック状態の管理
	StuckState stuck_{};

	// 攻撃の設定
	AttackSettings attackSettings_{};

	// 攻撃の状態管理
	AttackState attackState_{};

	// アニメーションの設定
	AnimationSettings animation_{};

	// アニメーションの状態管理
	AnimationStateData animationState_{};

	// 衝突の状態管理
	CollisionState collision_{};

	// 徘徊の状態管理
	WanderState wander_{};

	// 近接攻撃のコントローラー
	MeleeAttackController attackController_{};

	// 経路探索のナビゲーター
	EnemyAStarNavigator navigator_{};

	// スポーン位置（初期位置として使用、必要に応じてリスポーンなどにも使用）
	K4E::Vector3 spawnPosition_{};

	// ステージの床のAABBのリストへのポインタ（外部で管理されているものを参照する形）
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;

	// 壁の障害物のAABBのリストへのポインタ（外部で管理されているものを参照する形）
	const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;

	// デバッグ用の現在のアニメーション状態の名前
	const char* currentBehaviorName_ = "None";
};
