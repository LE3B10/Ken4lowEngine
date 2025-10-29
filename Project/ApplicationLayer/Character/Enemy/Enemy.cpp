#define NOMINMAX
#include "Enemy.h"
#include "CollisionTypeIdDef.h"
#include <Player.h>
#include "LevelObjectManager.h"  

#include <AABB.h>                
#include <cfloat>                // FLT_MAX
#include <cmath>
#include <imgui_widgets.cpp>
#include <random>

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
	if (body_.object) BaseCharacter::ApplySkinTo(body_.object.get(), skinTexturePath_);
	for (auto& part : parts_) if (part.object) BaseCharacter::ApplySkinTo(part.object.get(), skinTexturePath_);

	float idleRad = idlePoseAngleDeg * std::numbers::pi_v<float> / 180.0f;
	parts_[leftArmIndex_].transform.rotate_.x = idleRad;
	parts_[rightArmIndex_].transform.rotate_.x = idleRad;

	aiState_ = AIState::SpawnDelay; // 初期状態を出現待機に設定
	stateTimer_ = 0.0f;           // 状態タイマーリセット
	isActive_ = false;            // スポーン済みフラグリセット
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Enemy::Update(float deltaTime)
{
	// 攻撃クールダウンタイマーを進める
	if (attackCooldownTimer_ > 0.0f)
	{
		attackCooldownTimer_ -= deltaTime;
		if (attackCooldownTimer_ < 0.0f) attackCooldownTimer_ = 0.0f;
	}

	// プレイヤーが死んでいたら追跡・攻撃は中断して徘徊に戻す
	if (player_ && player_->IsDeadNow())
	{
		if (aiState_ == AIState::Chase || aiState_ == AIState::Attack) {
			aiState_ = AIState::Wander;
			stateTimer_ = 0.0f;
			didHitThisAttack_ = false; // 念のため攻撃フラグもリセット
		}
	}

	// いまの位置を覚えておく（衝突解決の基準になる）
	Vector3 oldPos = body_.transform.translate_;

	// ステートマシン
	switch (aiState_)
	{
	case AIState::SpawnDelay: UpdateSpawnDelay(deltaTime); break;
	case AIState::Wander:     UpdateWander(deltaTime);     break;
	case AIState::Chase:      UpdateChase(deltaTime);      break;
	case AIState::Attack:     UpdateAttack(deltaTime);     break;
	}

	SolveWorldCollision(oldPos);

	// ベースキャラクターの更新
	BaseCharacter::Update(deltaTime);
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Enemy::Draw()
{
	// スポーン前（SpawnDelay状態）は描画しない
	if (!isActive_) return;

	// ベースキャラクター描画
	BaseCharacter::Draw();
}

/// -------------------------------------------------------------
///				　			　 ImGui描画処理
/// -------------------------------------------------------------
void Enemy::DrawImGui()
{
	ImGui::Begin("Enemy Info");
	// 敵固有の情報表示
	ImGui::Text("AI State: ");
	switch (aiState_)
	{
	case AIState::SpawnDelay: ImGui::Text("SpawnDelay"); break;
	case AIState::Idle:       ImGui::Text("Idle");       break;
	case AIState::Wander:     ImGui::Text("Wander");     break;
	case AIState::Chase:      ImGui::Text("Chase");      break;
	case AIState::Attack:     ImGui::Text("Attack");     break;
	case AIState::Damaged:    ImGui::Text("Damaged");    break;
	case AIState::Dead:       ImGui::Text("Dead");       break;
	};

	// スポーン済みかどうか
	ImGui::Text("Is Active: %s", isActive_ ? "True" : "False");

	// 接触記録数
	ImGui::Text("Contact Records: %zu", contactRecord_.GetRecordCount());

	// 攻撃時の角度を調整
	ImGui::SliderFloat("Idle Pose Angle Deg", &idlePoseAngleDeg, -90.0f, 90.0f);
	ImGui::SliderFloat("Raise Angle Deg", &raiseAngleDeg, -180.0f, 0.0f);
	ImGui::SliderFloat("Hit Angle Deg", &hitAngleDeg, -180.0f, 0.0f);
	ImGui::SliderFloat("Return Angle Deg", &returnAngleDeg, -90.0f, 90.0f);

	ImGui::End();
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

		// 弾丸と衝突したときの処理
		OutputDebugStringA("Enemy hit by bullet!\n");
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

/// -------------------------------------------------------------
///				　　　状態遷移処理
/// -------------------------------------------------------------
void Enemy::TransitionState(AIState newState)
{
	(void)newState;
}

/// -------------------------------------------------------------
///				　　　出現待機状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateSpawnDelay(float deltaTime)
{
	stateTimer_ += deltaTime;
	if (stateTimer_ >= delayDuration_)
	{
		isActive_ = true; // スポーン済みに設定
		aiState_ = AIState::Wander; // 徘徊状態へ遷移
		stateTimer_ = 0.0f; // タイマーリセット
	}
}

/// -------------------------------------------------------------
///				　　　待機状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateIdle(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///				　　　徘徊状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateWander(float deltaTime)
{
	// 1) 残り時間を減らして、0以下なら新しい向きを決める
	wanderTimer_ -= deltaTime;
	if (wanderTimer_ <= 0.0f) {
		PickNewWanderDirection();
	}

	// 2) 現在yawを wanderTargetYaw_ にゆっくり近づける（最短回頭）
	{
		float& yawNow = body_.transform.rotate_.y;
		float  yawDst = wanderTargetYaw_;

		float twoPi = 2.0f * std::numbers::pi_v<float>;
		float diff = std::fmod(yawDst - yawNow, twoPi);
		if (diff > std::numbers::pi_v<float>) diff += twoPi;
		if (diff < -std::numbers::pi_v<float>) diff -= twoPi;

		float maxTurn = wanderTurnSpeed_ * deltaTime;
		if (std::fabs(diff) <= maxTurn) {
			yawNow = yawDst;
		}
		else {
			yawNow += (diff > 0.0f ? +maxTurn : -maxTurn);
		}
	}

	// 3) いま向いてる方向に歩く
	float yaw = body_.transform.rotate_.y;
	Vector3 forward = { -std::sinf(yaw), 0.0f, std::cosf(yaw) };
	Vector3 before = body_.transform.translate_;
	body_.transform.translate_ += forward * walkSpeed_;

	// 4) ほとんど動けなかったら詰まってると判断→すぐ方向再抽選
	{
		Vector3 moved = body_.transform.translate_ - before;
		float movedLenSq = moved.x * moved.x + moved.z * moved.z;
		if (movedLenSq < (stuckThreshold_ * stuckThreshold_)) {
			PickNewWanderDirection();
		}
	}

	// 5) プレイヤー生存＆近距離ならChaseへ移行するロジックは今まで通りでOK
	if (player_ && !player_->IsDeadNow()) {
		Vector3 toPlayer = player_->GetCenterPosition() - body_.transform.translate_;
		float distToPlayer = Vector3::Length(toPlayer);
		if (distToPlayer <= detectRadius_) {
			aiState_ = AIState::Chase;
			stateTimer_ = 0.0f;
			return;
		}
	}

	// 6) 徘徊時の腕ポーズはゾンビのidle角度にしておく
	float idleRad = idlePoseAngleDeg * std::numbers::pi_v<float> / 180.0f;
	if (leftArmIndex_ < parts_.size() && parts_[leftArmIndex_].object) {
		parts_[leftArmIndex_].transform.rotate_.x = idleRad;
	}
	if (rightArmIndex_ < parts_.size() && parts_[rightArmIndex_].object) {
		parts_[rightArmIndex_].transform.rotate_.x = idleRad;
	}
}

/// -------------------------------------------------------------
///				　　　追跡状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateChase(float deltaTime)
{
	(void)deltaTime;
	if (!player_) return;

	// プレイヤーが死んでいたら徘徊に戻る
	if (player_->IsDeadNow())
	{
		aiState_ = AIState::Wander;
		stateTimer_ = 0.0f;
		return;
	}

	Vector3 playerPos = player_->GetCenterPosition();
	Vector3 enemyPos = body_.transform.translate_;

	Vector3 diff = playerPos - enemyPos; diff.y = 0.0f;
	float  dist = std::max(0.0001f, Vector3::Length(diff));
	Vector3 dir = diff / dist;

	const float minDist = personalSpaceRadius_ + playerSpaceRadius_;      // 例: 1.6
	const float triggerDist = std::max(attackRange_, minDist + attackReachMargin_); // ★ここが肝

	// 常にプレイヤーの方を向く
	body_.transform.rotate_.y = std::atan2f(-dir.x, dir.z);

	// 近づくが、minDist を踏み越えない
	if (dist > minDist)
	{
		float step = chaseSpeed_;
		if (dist - step < minDist) step = dist - minDist;
		if (step > 0.0f) body_.transform.translate_ += dir * step;
	}
	// dist <= minDist のときは下がらない（押されない）

	// 移動「後」の距離で判定し直す
	Vector3 d2 = player_->GetCenterPosition() - body_.transform.translate_;
	d2.y = 0.0f;
	float distAfter = Vector3::Length(d2);

	if (distAfter <= triggerDist && attackCooldownTimer_ <= 0.0f)
	{
		aiState_ = AIState::Attack;
		stateTimer_ = 0.0f;
		didHitThisAttack_ = false;
		return;
	}

	if (distAfter > detectRadius_ * 1.5f) {
		aiState_ = AIState::Wander; stateTimer_ = 0.0f; return;
	}
}

/// -------------------------------------------------------------
///				　　　攻撃状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateAttack(float deltaTime)
{
	if (!player_) {
		aiState_ = AIState::Wander;
		return;
	}

	// プレイヤーが死んでいたら徘徊に戻る
	if (player_->IsDeadNow()) {
		aiState_ = AIState::Wander;
		stateTimer_ = 0.0f;
		didHitThisAttack_ = false;
		return;
	}

	// 距離と向きは今までどおり
	Vector3 to = player_->GetCenterPosition() - body_.transform.translate_;
	to.y = 0.0f;
	float dist = Vector3::Length(to);
	if (dist < 0.0001f) dist = 0.0001f;
	Vector3 dir = to / dist;

	// 顔はターゲットを見る（いまのコードと同じ）
	body_.transform.rotate_.y = std::atan2f(-dir.x, dir.z);

	// ※ここで "押し戻り" はあえてほぼしない（後で詳しく話す）
	// ただしめり込みがエグいほど近いときだけ、ほんの少しだけ離すのはアリ
	const float minDist = personalSpaceRadius_ + playerSpaceRadius_;
	const float keep = minDist * 0.6f;
	if (dist < keep) {
		body_.transform.translate_ -= dir * (keep - dist);
		dist = keep; // 計算をこの距離で続行
	}

	// 攻撃タイマーを進める
	stateTimer_ += deltaTime;
	float t = stateTimer_;

	// -------- アニメ用のフェーズ進捗を計算 --------
	// 0→1で腕を上げる
	float up01 = std::clamp(t / attackWindup_, 0.0f, 1.0f);
	// 0→1で腕を振り下ろす（windup後からカウント）
	float swing01 = std::clamp((t - attackWindup_) / attackSwing_, 0.0f, 1.0f);
	// 0→1で腕を戻す（windup+swing後からカウント）
	float rec01 = std::clamp(
		(t - attackWindup_ - attackSwing_) / attackRecover_,
		0.0f, 1.0f
	);

	// 腕の角度（シンプルにx回転だけでOK）
	auto deg2rad = [](float d) { return d * std::numbers::pi_v<float> / 180.0f; };

	// 基本は腕を前に振り上げるように -70度くらい
	float raisedAngle = deg2rad(raiseAngleDeg);

	float armAngleRad = 0.0f;
	if (t < attackWindup_)
	{
		// 溜め: 徐々に腕を上げる
		armAngleRad = raisedAngle * up01;
	}
	else if (t < attackWindup_ + attackSwing_)
	{
		// 振り下ろし: 上げ角度→ちょっと下方向(-10度くらい)へ
		float hitAngle = deg2rad(hitAngleDeg);
		armAngleRad = std::lerp(raisedAngle, hitAngle, swing01);
	}
	else
	{
		// リカバー:
		//   振り下ろし後の腕の角度(hit / return寄り) から
		//   ふだんのゾンビ構え(idlePoseAngleDeg) へ戻す
		float fromDeg = returnAngleDeg;      // 例: -10.0f
		float toDeg = idlePoseAngleDeg;    // 例: -70.0f ← 初期ポーズと同じ角度
		float curDeg = std::lerp(fromDeg, toDeg, rec01);

		armAngleRad = curDeg * std::numbers::pi_v<float> / 180.0f;
	}

	// 両腕を回転させる（parts_[leftArmIndex_] / parts_[rightArmIndex_]）
	if (leftArmIndex_ < parts_.size() && parts_[leftArmIndex_].object)
	{
		parts_[leftArmIndex_].transform.rotate_.x = armAngleRad;
	}
	if (rightArmIndex_ < parts_.size() && parts_[rightArmIndex_].object)
	{
		parts_[rightArmIndex_].transform.rotate_.x = armAngleRad;
	}

	// -------- 実ダメージとノックバック --------
	// 今のコードは「UpdateAttack()が始まった瞬間に毎回 player_->TakeDamage() を呼んでる」ので、
	// 見た目と当たりのタイミングがズレてる。:contentReference[oaicite:5]{index=5}
	//
	// swingフェーズが始まった瞬間（=腕を振り下ろし始めた瞬間）に1回だけヒットさせる
	if (!didHitThisAttack_ && t >= attackWindup_ && t < attackWindup_ + attackSwing_)
	{
		player_->ApplyDamageImpulse(dir, attackPower_, 0.15f);
		player_->TakeDamage(attackDamage_);

		didHitThisAttack_ = true;
		attackCooldownTimer_ = attackCooldown_; // クールダウン開始 :contentReference[oaicite:6]{index=6}
	}

	// -------- 攻撃が終わったらChaseに戻る --------
	float totalTime = attackWindup_ + attackSwing_ + attackRecover_;
	if (t >= totalTime)
	{
		// 腕をアイドル角度に戻す
		float idleRad = idlePoseAngleDeg * std::numbers::pi_v<float> / 180.0f;
		if (leftArmIndex_ < parts_.size() && parts_[leftArmIndex_].object) {
			parts_[leftArmIndex_].transform.rotate_.x = idleRad;
		}
		if (rightArmIndex_ < parts_.size() && parts_[rightArmIndex_].object) {
			parts_[rightArmIndex_].transform.rotate_.x = idleRad;
		}

		// 次の状態へ
		aiState_ = AIState::Chase;
		stateTimer_ = 0.0f;
	}
}

/// -------------------------------------------------------------
///				　　　ダメージ状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateDamaged(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///				　　　死亡状態の更新処理
/// -------------------------------------------------------------
void Enemy::UpdateDead(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///				　　　ワールド衝突解決処理
/// -------------------------------------------------------------
void Enemy::SolveWorldCollision(const Vector3& oldTranslate)
{
	if (!levelObjectManager_) {
		// ステージ情報がなければ何もしない
		return;
	}

	// プレイヤーと同じ当たり判定の前提:
	//  - コライダー半サイズ 0.8,2.0,0.8 は Enemy::Initialize() でも使っている値
	//    Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f }); :contentReference[oaicite:7]{index=7}
	const Vector3 half = { 0.8f, 2.0f, 0.8f };
	const float kEps = 0.002f;

	// プレイヤーと同じく「見た目の原点」と「物理中心」にオフセット差があるのでそろえる
	// Player側では kCenterOffset = {0,0.25f,0} を使って、
	// body_.transform.translate_ - kCenterOffset を物理中心扱いにしている。:contentReference[oaicite:8]{index=8}
	const Vector3 kCenterOffset = { 0.0f, 0.0f, 0.0f };

	// old/new の中心
	Vector3 oldCenter = oldTranslate - kCenterOffset;
	Vector3 newCenter = body_.transform.translate_ - kCenterOffset;

	// ワールドの当たり判定AABB群を取得
	const auto worldAABBs = levelObjectManager_->GetWorldAABBs();

	auto makeAABB = [&](const Vector3& c) {
		return AABB{ c - half, c + half };
		};

	Vector3 fixedCenter = oldCenter;

	// プレイヤーのMove()と同じロジック：
	// X → Z → Y の順番で軸ごとに押し戻す。:contentReference[oaicite:9]{index=9}
	auto resolveAxis = [&](int axis, float delta)
		{
			if (delta == 0.0f) { return; }

			if (axis == 0) fixedCenter.x += delta;
			if (axis == 1) fixedCenter.y += delta;
			if (axis == 2) fixedCenter.z += delta;

			AABB p = makeAABB(fixedCenter);

			bool hit = false;
			float bestFix = 0.0f;
			float bestDist = FLT_MAX;

			for (const auto& w : worldAABBs)
			{
				// AABB同士が交差しているかどうかをチェック（プレイヤーと同じ式）:contentReference[oaicite:10]{index=10}
				if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
					p.min.y <= w.max.y && p.max.y >= w.min.y &&
					p.min.z <= w.max.z && p.max.z >= w.min.z)) {
					continue;
				}

				float cand = 0.0f;
				bool valid = false;

				if (axis == 0)
				{
					// X方向押し戻し
					if (oldCenter.x + half.x <= w.min.x) { cand = (w.min.x - half.x) - kEps; valid = true; }
					else if (oldCenter.x - half.x >= w.max.x) { cand = (w.max.x + half.x) + kEps; valid = true; }
					else {
						float dMin = fabsf((w.min.x - half.x) - oldCenter.x);
						float dMax = fabsf((w.max.x + half.x) - oldCenter.x);
						cand = (dMin <= dMax) ?
							(w.min.x - half.x - kEps) :
							(w.max.x + half.x + kEps);
						valid = true;
					}
					if (valid) {
						float dist = fabsf(cand - fixedCenter.x);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else if (axis == 2)
				{
					// Z方向押し戻し（ほぼXと同じロジックのZ版）:contentReference[oaicite:11]{index=11}
					if (oldCenter.z + half.z <= w.min.z) { cand = (w.min.z - half.z) - kEps; valid = true; }
					else if (oldCenter.z - half.z >= w.max.z) { cand = (w.max.z + half.z) + kEps; valid = true; }
					else {
						float dMin = fabsf((w.min.z - half.z) - oldCenter.z);
						float dMax = fabsf((w.max.z + half.z) - oldCenter.z);
						cand = (dMin <= dMax) ?
							(w.min.z - half.z - kEps) :
							(w.max.z + half.z + kEps);
						valid = true;
					}
					if (valid) {
						float dist = fabsf(cand - fixedCenter.z);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else // axis == 1 (Y)
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

	// コライダー中心も同期（プレイヤーと同じく物理中心ベースで渡す）:contentReference[oaicite:12]{index=12}
	Collider::SetCenterPosition(fixedCenter);
}

void Enemy::PickNewWanderDirection()
{
	// 静的な乱数エンジンを1個だけ確保してずっと使い回す
	// （毎回 new しない＆毎回seedしないのがポイント）
	static thread_local std::mt19937 rng{ std::random_device{}() };

	// 0.0f 〜 1.0f の一様乱数
	std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

	// ランダムな方向(0〜2π)
	float twoPi = 2.0f * std::numbers::pi_v<float>;
	float r01 = dist01(rng);
	wanderTargetYaw_ = r01 * twoPi;

	// ランダムな徘徊持続時間（wanderChangeIntervalMin_〜Max_）
	std::uniform_real_distribution<float> distTime(wanderChangeIntervalMin_, wanderChangeIntervalMax_);
	wanderTimer_ = distTime(rng);
}
