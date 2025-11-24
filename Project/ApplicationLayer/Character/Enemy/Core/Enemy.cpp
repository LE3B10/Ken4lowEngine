#define NOMINMAX
#include "Enemy.h"
#include "CollisionTypeIdDef.h"
#include <Player.h>
#include "LevelObjectManager.h"  

#include <EnemyAIDeadState.h>
#include <EnemyAIDamagedState.h>
#include <EnemyAIAttackState.h>
#include <EnemyAIChaseState.h>
#include <EnemyAIIdleState.h>
#include <EnemyAISpawnState.h>
#include <EnemyBehaviorTreeBuilder.h>

#include <AABB.h>                
#include <cfloat>                // FLT_MAX
#include <cmath>
#include <random>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

/// -------------------------------------------------------------
///					　デストラクタ
/// -------------------------------------------------------------
Enemy::~Enemy()
{
	// 接触記録クリア
	contactRecord_.Clear();
}

/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void Enemy::Initialize()
{
	// ベースキャラクター初期化
	BaseCharacter::Initialize();
	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));
	Collider::SetOwner<Enemy>(this);
	Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f });

	// テクスチャの設定
	BaseCharacter::ApplySkinToAllParts(skinTexturePath_);

	float idleRad = wander_.idlePoseAngleDeg * std::numbers::pi_v<float> / 180.0f;
	parts_[partIndices_.leftArm].transform.rotate_.x = idleRad;
	parts_[partIndices_.rightArm].transform.rotate_.x = idleRad;

	hp_ = maxHp_;                  // 体力満タンに
	stateTimer_ = 0.0f;           // 状態タイマーリセット
	death_ = DeathEnemyState{};
	hasAggro_ = false;

	// パーツの親も復元しておく（分解で parent_ を外している場合）
	for (auto& part : parts_) {
		part.transform.parent_ = &body_.transform;
	}

	// 最初の状態をセット （Loading）
	ChangeState(std::make_unique<EnemyAISpawnState>());
	currentStateId_ = AIState::Spawn; // 最初のステート
	isActive_ = false;                 // まだ出現前

	// ビヘイビアツリー初期化
	//InitializeBehaviorTree();
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Enemy::Update(float deltaTime)
{
	// もう完全に死亡処理が終わっていたら何もしない
	if (death_.finished) return;

	// 死亡演出中なら DeadState に任せる（BaseCharacter のアニメは OFF）
	if (death_.active)
	{
		if (currentState_) currentState_->Update(this, deltaTime);
		Collider::SetOBBHalfSize({ 0.0f, 0.0f, 0.0f }); // 衝突判定無効化
		BaseCharacter::Update(deltaTime);
		return;
	}

	// 移動前の位置を保存
	Vector3 oldPos = body_.transform.translate_;

	// Damaged 中は BT を止めてステートだけ
	if (currentStateId_ == AIState::Spawn || currentStateId_ == AIState::Damaged)
	{
		if (currentState_) currentState_->Update(this, deltaTime);
	}
	else
	{
		// 生きている間だけ BT + ステート + アニメ
		//if (behaviorTree_) behaviorTree_->Tick(*this, deltaTime);
		if (currentState_)  currentState_->Update(this, deltaTime);
	}

	// 移動後に必ずワールドとの衝突を解決
	SolveWorldCollision(oldPos);

	// コライダー中心を同期
	Collider::SetCenterPosition(GetCenterPosition());

	// ベースキャラクターの更新
	BaseCharacter::Update(deltaTime);
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Enemy::Draw()
{
	// スポーン前（SpawnDelay状態）は描画しない
	if (!isActive_ && !death_.active) return;

	// ベースキャラクター描画
	BaseCharacter::Draw();
}

/// -------------------------------------------------------------
///				　			　 ImGui描画処理
/// -------------------------------------------------------------
void Enemy::DrawImGui()
{
#ifdef USE_IMGUI

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　　　 衝突時に呼ばれる仮想関数
/// -------------------------------------------------------------
void Enemy::OnCollision(Collider* other)
{
	uint32_t serialNumber = other->GetUniqueID(); // 相手のシリアルナンバー取得

	// 弾丸と衝突したときの処理
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		// 接触記録があれば何もせず抜ける
		if (contactRecord_.Check(serialNumber)) return;

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		// 攻撃されたので警戒開始
		hasAggro_ = true;

		ChangeState(std::make_unique<EnemyAIDamagedState>()); // ダメージ状態へ変更

		// 弾丸と衝突したときの処理
		OutputDebugStringA("Enemy hit by bullet!\n");
	}
}

/// -------------------------------------------------------------
///				プレイヤーが攻撃範囲内にいるかどうか
/// -------------------------------------------------------------
bool Enemy::IsPlayerInAttackRange() const
{
	if (!player_) return false;

	Vector3 toPlayer = player_->GetCenterPosition() - GetCenterPosition();
	float dist = Vector3::Length(toPlayer);

	// 敵とプレイヤーの最接近距離（これ以上近づけない距離）
	float minDist = GetPersonalSpaceRadius(); // ≒1.6

	// 実際に「攻撃に入っていい距離」
	//  - range がそれより長ければ range を優先
	//  - そうでなければ「最接近距離 + ちょっと余裕(reachMargin)」
	float triggerDist = std::max(attack_.range, minDist + attack_.reachMargin);

	// クールダウン中は攻撃に入らない
	if (attack_.cooldownTimer > 0.0f) {
		return false;
	}

	// この距離以内なら「攻撃可能」とみなす
	return dist <= triggerDist;
}

/// -------------------------------------------------------------
///				プレイヤーが視認範囲内にいるかどうか
/// -------------------------------------------------------------
bool Enemy::CanSeePlayer() const
{
	if (!player_) return false;

	Vector3 toPlayer = player_->GetCenterPosition() - GetCenterPosition();
	float dist = Vector3::Length(toPlayer);

	// 1) 近づかれたら、とりあえず気付く（背後からでも）
	if (dist <= wander_.detectRadius)
	{
		return true;
	}

	// 2) あまりにも遠いときは見えない
	float viewDist = (config_.viewDistance > 0.0f)
		? config_.viewDistance
		: wander_.detectRadius; // viewDistance 未設定なら detectRadius を使う

	if (dist > viewDist)
	{
		// すでに警戒状態なら、「ちょっと遠くまで追う」ようにしたければここで調整
		if (!hasAggro_) {
			return false;
		}
	}

	// 3) 一度攻撃されて警戒中なら、距離さえOKなら「見えている」扱いにする
	if (hasAggro_)
	{
		return true;
	}

	// 4) まだ警戒していないときだけ視野角で判定
	Vector3 forward = { 0.0f, 0.0f, 1.0f }; // 本当は body_.transform の向きから計算するとベスト
	Vector3 dir = Vector3::Normalize(toPlayer);
	float dot = Vector3::Dot(forward, dir);

	float halfAngleRad = (config_.viewAngle * 0.5f) * (3.14159265f / 180.0f);
	float cosHalf = std::cos(halfAngleRad);

	return dot >= cosHalf;
}

/// -------------------------------------------------------------
///				　　　プレイヤーに向かって移動する
/// -------------------------------------------------------------
void Enemy::MoveTowardPlayer(float deltaTime)
{
	if (!player_) return;
	(void)deltaTime;
	RequestChaseState();
}

/// -------------------------------------------------------------
///				　　　		徘徊行動
/// -------------------------------------------------------------
void Enemy::Wander(float deltaTime)
{
	(void)deltaTime;

	RequestWanderState();
}

/// -------------------------------------------------------------
///				　　　		攻撃行動
/// -------------------------------------------------------------
void Enemy::Attack()
{
	RequestAttackState();
}

/// -------------------------------------------------------------
///				　　　状態変更処理
/// -------------------------------------------------------------
void Enemy::ChangeState(std::unique_ptr<IEnemyAIState> newState)
{
	// 今のステートから抜ける
	if (currentState_) {
		currentState_->Exit(this);
	}

	// ステート差し替え
	currentState_ = std::move(newState);

	// 新しいステートに入る
	if (currentState_) {
		currentState_->Enter(this);
	}
}

/// -------------------------------------------------------------
///				　　　中心座標を取得する純粋仮想関数
/// -------------------------------------------------------------
Vector3 Enemy::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,0.0f,0.0f };
	return body_.transform.translate_ + offset;
}

void Enemy::ApplyStageParams(float hp, float walkSpeed, float chaseSpeed, float attackDamage, float attackCooldown, float detectRadius)
{
	maxHp_ = hp;
	walkSpeed_ = walkSpeed;
	chaseSpeed_ = chaseSpeed;
	attack_.damage = attackDamage;
	attack_.cooldown = attackCooldown;
	wander_.detectRadius = detectRadius;
}

/// -------------------------------------------------------------
///				　　　ビヘイビアツリー初期化
/// -------------------------------------------------------------
void Enemy::InitializeBehaviorTree()
{
	behaviorTree_ = EnemyBehaviorTreeBuilder::BuildBehaviorTree(type_);
}

/// -------------------------------------------------------------
///				　　　ワールド衝突解決処理
/// -------------------------------------------------------------
void Enemy::SolveWorldCollision(const Vector3& oldTranslate)
{
	// ステージ情報がなければ何もしない
	if (!levelObjectManager_) return;

	// プレイヤーと同じ当たり判定の前提:
	const Vector3 half = { 0.8f, 2.0f, 0.8f };
	const float kEps = 0.002f;

	// プレイヤーと同じく「見た目の原点」と「物理中心」にオフセット差があるのでそろえる
	const Vector3 kCenterOffset = { 0.0f, 0.0f, 0.0f };

	// old/new の中心
	Vector3 oldCenter = oldTranslate - kCenterOffset;
	Vector3 newCenter = body_.transform.translate_ - kCenterOffset;

	// ワールドの当たり判定AABB群を取得
	const auto worldAABBs = levelObjectManager_->GetWorldAABBs();

	// プレイヤーAABBを作るラムダ
	auto makeAABB = [&](const Vector3& c) {return AABB{ c - half, c + half };	};

	// 押し戻し後の中心位置
	Vector3 fixedCenter = oldCenter;

	// 指定軸方向の押し戻しを解決するラムダ
	auto resolveAxis = [&](int axis, float delta)
		{
			// 動いていなければ何もしない
			if (delta == 0.0f) { return; }

			if (axis == 0) fixedCenter.x += delta;
			if (axis == 1) fixedCenter.y += delta;
			if (axis == 2) fixedCenter.z += delta;

			// プレイヤーAABBを作成
			AABB p = makeAABB(fixedCenter);

			bool hit = false;
			float bestFix = 0.0f;
			float bestDist = FLT_MAX;

			// ワールドAABB群と当たり判定
			for (const auto& w : worldAABBs)
			{
				// AABB同士が交差しているかどうかをチェック（プレイヤーと同じ式）
				if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
					p.min.y <= w.max.y && p.max.y >= w.min.y &&
					p.min.z <= w.max.z && p.max.z >= w.min.z)) {
					continue; // 交差していなければ次へ
				}

				float cand = 0.0f;
				bool valid = false;

				// 押し戻し候補を軸ごとに計算
				if (axis == 0)
				{
					// X方向押し戻し
					if (oldCenter.x + half.x <= w.min.x) { cand = (w.min.x - half.x) - kEps; valid = true; }
					else if (oldCenter.x - half.x >= w.max.x) { cand = (w.max.x + half.x) + kEps; valid = true; }
					else
					{
						float dMin = fabsf((w.min.x - half.x) - oldCenter.x);
						float dMax = fabsf((w.max.x + half.x) - oldCenter.x);
						cand = (dMin <= dMax) ?
							(w.min.x - half.x - kEps) :
							(w.max.x + half.x + kEps);
						valid = true;
					}

					// 最も近い押し戻し候補を採用
					if (valid)
					{
						float dist = fabsf(cand - fixedCenter.x);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else if (axis == 2)
				{
					// Z方向押し戻し（ほぼXと同じロジックのZ版）
					if (oldCenter.z + half.z <= w.min.z) { cand = (w.min.z - half.z) - kEps; valid = true; }
					else if (oldCenter.z - half.z >= w.max.z) { cand = (w.max.z + half.z) + kEps; valid = true; }
					else
					{
						float dMin = fabsf((w.min.z - half.z) - oldCenter.z);
						float dMax = fabsf((w.max.z + half.z) - oldCenter.z);
						cand = (dMin <= dMax) ?
							(w.min.z - half.z - kEps) :
							(w.max.z + half.z + kEps);
						valid = true;
					}
					if (valid)
					{
						float dist = fabsf(cand - fixedCenter.z);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else
				{
					// Y方向押し戻し（床/天井処理）
					if (oldCenter.y - half.y >= w.max.y) { cand = (w.max.y + half.y) + kEps; valid = true; }     // 床の上に乗る
					else if (oldCenter.y + half.y <= w.min.y) { cand = (w.min.y - half.y) - kEps; valid = true; } // 天井の下で止まる
					else {
						float dFloor = fabsf((w.max.y + half.y) - oldCenter.y);
						float dCeil = fabsf((w.min.y - half.y) - oldCenter.y);
						cand = (dFloor <= dCeil) ?
							(w.max.y + half.y + kEps) :
							(w.min.y - half.y - kEps);
						valid = true;
					}
					if (valid) {
						float dist = fabsf(cand - fixedCenter.y);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
			}

			// 押し戻しが発生していれば位置を修正
			if (hit)
			{
				if (axis == 0) fixedCenter.x = bestFix;
				if (axis == 2) fixedCenter.z = bestFix;
				if (axis == 1) fixedCenter.y = bestFix;
			}
		};

	// 軸ごとに、"どれだけ動いたか" を解決
	resolveAxis(0, newCenter.x - oldCenter.x);
	resolveAxis(2, newCenter.z - oldCenter.z);
	resolveAxis(1, newCenter.y - oldCenter.y);

	// 最終的な位置を反映
	body_.transform.translate_ = fixedCenter + kCenterOffset;

	// コライダー中心も同期（プレイヤーと同じく物理中心ベースで渡す）
	Collider::SetCenterPosition(fixedCenter);
}

void Enemy::RequestSpawnState()
{
	if (currentStateId_ == AIState::Spawn) return; // すでに Spawn 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Spawn);
	ChangeState(std::make_unique<EnemyAISpawnState>());
}

void Enemy::RequestIdleState()
{
	if (currentStateId_ == AIState::Idle) return; // すでに Idle 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Idle);
	ChangeState(std::make_unique<EnemyAIIdleState>());
}

void Enemy::RequestChaseState()
{
	if (currentStateId_ == AIState::Chase) return; // すでに Chase 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Chase);
	ChangeState(std::make_unique<EnemyAIChaseState>());
}

void Enemy::RequestAttackState()
{
	if (currentStateId_ == AIState::Attack) return; // すでに Attack 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Attack);
	ChangeState(std::make_unique<EnemyAIAttackState>());
}

void Enemy::RequestWanderState()
{
	if (currentStateId_ == AIState::Wander) return; // すでに Wander 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Wander);
	ChangeState(std::make_unique<EnemyAIWanderState>()); // とりあえず IdleState を使う
}

void Enemy::RequestDamagedState()
{
	if (currentStateId_ == AIState::Damaged) return; // すでに Damaged 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Damaged);
	ChangeState(std::make_unique<EnemyAIDamagedState>());
}

void Enemy::RequestDeadState()
{
	if (currentStateId_ == AIState::Dead) return; // すでに Dead 状態なら何もしない

	ResetStateTimer();
	SetState(AIState::Dead);
	ChangeState(std::make_unique<EnemyAIDeadState>());
}
