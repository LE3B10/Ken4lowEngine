#pragma once

#include "EnemyBase.h"
#include "../AI/MeleeAttackController.h"
#include "../Navigation/EnemyAStarNavigator.h"
#include <string>
#include <filesystem>

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
		LungeScratch,	 // 踏み込みひっかき
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
		float minLungeForwardDistance = 1.6f; // 踏み込みひっかきの前進距離が有効になる最小距離
	};

	// 移動と回転の設定
	struct MoveSettings
	{
		float moveSpeed = 3.2f;								 // 基本移動速度
		float rotateSpeed = 8.0f;							 // 回転速度
		float maxResolvePushPerFrame = 0.75f;				 // ステージ衝突解決の最大押し戻し距離
		float maxHorizontalPushPerFrame = 0.45f;			 // 水平方向の衝突解決の最大押し戻し距離
		bool obstacleTopLandingEnabled = true;				 // 障害物上面への着地判定を有効にする
		float obstacleTopLandingTolerance = 0.35f;			 // 障害物上面への着地を許容する足元の高さ誤差
		float obstacleTopLandingMaxHeight = 3.5f;			 // 障害物上面へ着地可能な最大段差
		float obstacleTopLandingMinHorizontalOverlap = 0.2f; // 上面着地に必要なXZ最小重なり量
	};

	// 落下と重力の設定
	struct JumpSettings
	{
		bool enabled = true;				// ジャンプを有効にするかどうか
		float baseVelocity = 12.0f;			// ジャンプの基本垂直速度
		float maxVelocity = 18.0f;			// ジャンプの最大垂直速度
		float cooldown = 0.9f;				// ジャンプのクールダウン時間
	};

	// ジャンプの状態管理
	struct JumpState
	{
		float cooldownTimer = 0.0f;		 // ジャンプのクールダウンタイマー
		float appliedVelocity = 0.0f;	 // 実際に適用された垂直速度
		std::string lastReason = "None"; // ジャンプを試みた最後の理由（デバッグ用）
	};

	// 乗り越えの設定
	struct TraversalSettings
	{
		bool enabled = true;
		bool preferDirectClimb = true;
		float maxClimbHeight = 2.0f;
		float minClimbHeight = 0.15f;
		float maxClimbObstacleWidth = 3.0f;
		float maxClimbObstacleDepth = 3.0f;
		float climbJumpTriggerDistance = 2.0f;
		float climbHorizontalDistanceMax = 4.0f;
		float directClimbDistanceMax = 8.0f;
		float directLineWidth = 1.2f;
		bool allowJumpOverLowObstacles = true;
	};

	// 乗り越えの状態
	struct TraversalState
	{
		int climbableObstacleCount = 0;
		int blockingObstacleCount = 0;
		bool nearClimbableObstacle = false;
		bool directClimbCandidateFound = false;
		std::string lastReason = "None";
		std::string selectedObstacleJudgeReason = "None";
		std::string selectedObstacleRejectReason = "None";
		int selectedClimbObstacleIndex = -1;
		float selectedObstacleHeight = 0.0f;
		float selectedObstacleWidth = 0.0f;
		float selectedObstacleDepth = 0.0f;
		float selectedEnemyFootY = 0.0f;
		float selectedObstacleTopY = 0.0f;
		bool selectedObstacleClimbResult = false;
	};


	// 接触した障害物の状態
	struct ContactObstacleState
	{
		bool hasContact = false;
		bool climbable = false;
		bool climbableByHeight = false;
		bool rejectedByWidthDepth = false;
		bool rejectedByAABBSize = false;
		bool judgedByContactFace = false;
		int obstacleIndex = -1;
		K4E::AABB obstacleAABB{};
		float obstacleTopY = 0.0f;
		float obstacleHeightFromFoot = 0.0f;
		float obstacleWidth = 0.0f;
		float obstacleDepth = 0.0f;
		float obstacleForwardThickness = 0.0f;
		float contactFaceDistance = 0.0f;
		float enemyFootY = 0.0f;
		bool xzOverlapping = false;
		bool facingObstacle = false;
		std::string reason = "None";
		std::string possibleReason = "None";
		std::string notClimbableReason = "None";
	};

	// 接触ジャンプのデバッグ状態
	struct ContactJumpDebugState
	{
		bool calledThisFrame = false;
		bool everCalled = false;
		std::string lastReason = "NotCalled";
		float footY = 0.0f;
		float obstacleTopY = 0.0f;
		float obstacleHeightFromFoot = 0.0f;
		float plannedJumpVelocity = 0.0f;
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

	// 攻撃選択の設定
	struct AttackSelectSettings
	{
		bool enabled = true;
		bool useProbability = true;
		float lungeChance = 0.35f;
		float lungeMinDistance = 2.0f;
		float lungeMaxDistance = 3.6f;
		float lungeSelectCooldown = 2.0f;
		int maxConsecutiveLunge = 1;
		int maxConsecutiveScratch = 3;
		bool forceScratchAfterLunge = true;
	};

	// 攻撃選択のデバッグ状態
	struct AttackSelectState
	{
		float lungeSelectCooldownTimer = 0.0f;
		int consecutiveScratchCount = 0;
		int consecutiveLungeCount = 0;
		float lastRoll = 0.0f;
		float lastLungeChance = 0.0f;
		MeleeAttackType lastSelectedAttack = MeleeAttackType::Scratch;
		std::string lastReason = "None";
	};

	// アニメーションの設定
	struct ScratchAnimationSettings
	{
		float prepareArmX = -0.55f;
		float prepareArmY = 0.0f;
		float prepareArmZ = 0.25f;
		float strikeArmX = 1.10f;
		float strikeArmY = 0.0f;
		float strikeArmZ = -0.25f;
		float prepareEndRate = 0.25f;
		float strikeEndRate = 0.55f;
		float bodyPrepareLean = -0.03f;
		float bodyStrikeLean = 0.10f;
		float returnSpeed = 14.0f;
	};

	struct LungeAnimationSettings
	{
		float prepareArmX = -1.1f;
		float prepareArmY = 0.0f;
		float prepareArmZ = 0.0f;
		float strikeArmX = 1.35f;
		float strikeArmY = 0.0f;
		float strikeArmZ = -0.35f;
		float bodyPrepareLean = -0.08f;
		float bodyStrikeLean = 0.30f;
		float prepareEndRate = 0.35f;
		float strikeEndRate = 0.65f;
		float returnSpeed = 10.0f;
		float legStepAmount = 0.20f;
	};

	struct AnimationSettings
	{
		float visualYawOffset = 0.0f;	 // 見た目の向きのオフセット（攻撃中などに体を傾けるため）
		float walkAnimSpeed = 8.0f;		 // 歩行アニメーションの速度
		float walkArmSwing = 0.55f;		 // 歩行中の腕の振り幅
		float walkLegSwing = 0.45f;		 // 歩行中の脚の振り幅
		ScratchAnimationSettings scratch;
		LungeAnimationSettings lunge;
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
		float attackAnimProgress = 0.0f;						// 現在攻撃の正規化進行度（デバッグ表示用）
	};

	// Scratch攻撃の左右交互制御
	struct ScratchArmState
	{
		bool useLeftArm = true;			 // 今回のScratchで使う腕
		bool wasScratchAttacking = false; // Scratch開始検知用の前フレーム状態
	};

	// 頭の注視制御の設定
	struct HeadLookSettings
	{
		bool enabled = true;
		float yawLimitDeg = 90.0f;
		float pitchMinDeg = -35.0f;
		float pitchMaxDeg = 45.0f;
		float lerpSpeed = 12.0f;
	};

	// 頭の注視制御の状態
	struct HeadLookState
	{
		float currentYaw = 0.0f;
		float currentPitch = 0.0f;
		float targetYaw = 0.0f;
		float targetPitch = 0.0f;
		bool targetVisible = false;
		std::string reason = "Disabled";
	};

	// 被ダメージリアクションの設定
	struct HitReactionSettings
	{
		bool enabled = true;
		float duration = 0.18f;
		float knockbackPower = 2.0f;
		float knockbackUpPower = 0.5f;
		float bodyLean = -0.18f;
		float flashDuration = 0.12f;
		bool interruptAttack = false;
		bool stopBehaviorWhileActive = true;
	};

	// 被ダメージリアクションの状態
	struct HitReactionState
	{
		bool active = false;
		float timer = 0.0f;
		K4E::Vector3 knockbackDirection{};
		std::string lastReason = "None";
	};

	// 死亡演出の設定
	struct DeathAnimationSettings
	{
		bool enabled = true;
		float duration = 1.2f;
		float fallRotateX = 1.35f;
		float sinkDistance = 0.4f;
		float fadeDelay = 0.4f;
		float fadeDuration = 0.6f;
		bool disableCollisionOnDeath = true;
		bool stopMoveOnDeath = true;
	};

	// 死亡演出の状態
	struct DeathAnimationState
	{
		bool active = false;
		float timer = 0.0f;
		K4E::Vector3 startPosition{};
		K4E::Vector3 startRotation{};
		std::string lastReason = "None";
	};

	// 基本ステータスの設定
	struct BasicStatsSettings
	{
		int maxHp = 160;
		bool resetHpOnLoad = true;
	};

	// 個体間分離の設定
	struct SeparationSettings
	{
		bool enabled = true;
		float radius = 1.6f;
		float strength = 1.1f;
		float maxPushPerFrame = 0.28f;
		float attackPushScale = 0.5f;
		bool targetNearLateralEnabled = true;
		float targetNearLateralOffset = 0.25f;
		float targetNearLateralStrength = 0.65f;
	};

	// 個体間分離の状態管理
	struct SeparationState
	{
		int overlappingEnemyCount = 0;
		K4E::Vector3 lastSeparationPush{};
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
		bool landedOnObstacleTop = false;							 // 障害物上面への着地が成立したかどうか
		std::string lastObstacleTopLandingName = "None";			 // 最後に上面着地した障害物名（デバッグ用）
	};


	// 調整データの保存・読み込み結果表示
	struct TuningIoState
	{
		// MeleeEnemyのJSONはProject配下のResourcesに固定して、Project外のResources生成を防ぐ。
		std::filesystem::path jsonPath = "Resources/DataAssets/Enemy/MeleeEnemy/MeleeEnemy_Normal.json";
		std::string lastLoadResult = "未実行";
		std::string lastSaveResult = "未実行";
		int jsonFormatVersion = 2;
		int savedCategoryCount = 12;
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
	void TakeDamage(int amount, const K4E::Vector3& hitDir, float hitPower) override;

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
	void StartHitReaction(const K4E::Vector3& hitDirection);

	// 現在選択されている攻撃の種類を設定・取得
	void SetSelectedAttackType(MeleeAttackType type) { attackSettings_.selectedAttackType = type; }

	// 選択されている攻撃の種類を取得
	MeleeAttackType GetSelectedAttackType() const { return attackSettings_.selectedAttackType; }

	// 選択中個体のみ詳細デバッグ描画するための表示切り替えを設定する
	void SetDetailDebugDrawEnabled(bool enabled) { detailDebugDrawEnabled_ = enabled; }

	// 全体/選択のみの経路描画を切り替えるため、個体ごとに表示モードを受け取る。
	void SetPathDebugDrawEnabled(bool enabled) { pathDebugDrawEnabled_ = enabled; }

	// 全体表示時の軽量化のため、詳細デバッグ要素の描画を切り替える。
	void SetPathDetailDebugDrawEnabled(bool enabled) { pathDetailDebugDrawEnabled_ = enabled; }

	// 個体識別しやすいように経路色オフセットを設定する。
	void SetPathDebugColorOffset(float offset) { pathDebugColorOffset_ = offset; }

	// 選択中個体の強調表示を切り替える。
	void SetPathDebugSelected(bool selected) { pathDebugSelected_ = selected; }

	// 分離設定を取得する
	const SeparationSettings& GetSeparationSettings() const { return separationSettings_; }

	// 分離状態を取得する
	const SeparationState& GetSeparationState() const { return separationState_; }

	// 死亡状態かを取得する
	bool IsDeadEnemy() const { return IsDead(); }

	// 現フレームの分離状態をリセットする
	void BeginSeparationFrame();

	// 個体間分離の押し出しをXZ平面へ適用する
	K4E::Vector3 ApplySeparationPushXZ(const K4E::Vector3& desiredPush, float pushScale);

	// 個体間分離で重なり相手を検出した回数を加算する
	void AddSeparationOverlapCount(int count);

	// 複数体の行動状態確認用に現在行動名を返す
	const char* GetCurrentBehaviorName() const { return currentBehaviorName_; }

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

	// 踏み込みひっかきの前進距離が有効になる最小距離内にターゲットがいるかどうかを判定
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
	void TryJumpForTraversal(float deltaTime);

		// ジャンプを試みる処理（ターゲットの高さや距離、ジャンプのクールダウンなどを考慮してジャンプするかどうかを判断し、実際にジャンプする）
	bool TryJumpOverClimbableObstacle(float deltaTime);
	bool TryJumpOverContactObstacle();
	bool EvaluateContactObstacleClimbable(const K4E::AABB& obstacle, int index);
	bool TryDirectClimbOverObstacleToTarget(float deltaTime);

	// 乗り越え可能な障害物と回避対象障害物を分類する
	void UpdateTraversalObstacleClassification();

	// 障害物1個が乗り越え可能かを判定する
	bool IsObstacleClimbable(const K4E::AABB& obstacle, const K4E::Vector3& selfPos, const K4E::Vector3& moveOrTargetDir) const;

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
	void UpdateHitReaction(float deltaTime);
	void UpdateDeathAnimation(float deltaTime);
	void StartDeathAnimation();

	// アニメーションの処理（現在の状態に応じたアニメーションの更新や見た目の向きの調整など）
	void UpdateVisualAnimation(float deltaTime);

	// ステージの衝突を解決する処理（AABBとの衝突を解決して押し戻す）
	bool IsInsideStageBounds(const K4E::Vector3& position) const;

	// ステージの衝突を解決する処理（AABBとの衝突を解決して押し戻す）
	const char* GetAnimStateName() const;

	// 調整用パラメータのJSON読み込み処理
	bool LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage = nullptr);

	// 調整用パラメータのJSON保存処理
	bool SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage = nullptr) const;

	// 調整用パラメータをデフォルト値に戻す処理
	void ResetTuningToDefault();
	MeleeAttackType SelectAttackType(float distance);

private: /// ---------- メンバ変数 ---------- //

	// ターゲットのコライダーへのポインタ（プレイヤーなど）
	K4E::Collider* target_ = nullptr;

	// 各種設定と状態管理の構造体
	BasicStatsSettings basicStats_{};

	DetectionSettings detection_{};

	// 移動と回転の設定
	MoveSettings move_{};

	// 落下と重力の設定
	JumpSettings jump_{};

	// ジャンプの状態管理
	JumpState jumpState_{};

	// 乗り越え設定
	TraversalSettings traversal_{};

	// 乗り越え状態
	TraversalState traversalState_{};

	// 接触障害物状態
	ContactObstacleState contactObstacleState_{};

	// 接触ジャンプのデバッグ状態
	ContactJumpDebugState contactJumpDebugState_{};

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

	// 攻撃選択の設定
	AttackSelectSettings attackSelectSettings_{};

	// 攻撃選択のデバッグ状態
	AttackSelectState attackSelectState_{};

	// アニメーションの設定
	AnimationSettings animation_{};

	// アニメーションの状態管理
	AnimationStateData animationState_{};

	// Scratch攻撃の左右交互制御状態
	ScratchArmState scratchArmState_{};

	// 頭の注視設定
	HeadLookSettings headLookSettings_{};

	// 頭の注視状態
	HeadLookState headLookState_{};
	HitReactionSettings hitReactionSettings_{};
	HitReactionState hitReactionState_{};
	DeathAnimationSettings deathAnimationSettings_{};
	DeathAnimationState deathAnimationState_{};

	// 個体間分離の設定
	SeparationSettings separationSettings_{};

	// 個体間分離の状態
	SeparationState separationState_{};

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

	// 経路探索へ渡す回避対象AABB（乗り越え不可のみ）
	std::vector<K4E::AABB> pathBlockingObstacleAABBs_{};

	// 乗り越え候補AABB（上面着地・ジャンプ判定用）
	std::vector<K4E::AABB> climbableObstacleAABBs_{};

	// デバッグ用の現在のアニメーション状態の名前
	const char* currentBehaviorName_ = "None";
	bool detailDebugDrawEnabled_ = true;
	bool pathDebugDrawEnabled_ = true;
	bool pathDetailDebugDrawEnabled_ = true;
	bool pathDebugSelected_ = false;
	float pathDebugColorOffset_ = 0.0f;

	// 調整データのI/O状態
	TuningIoState tuningIo_{};
};
