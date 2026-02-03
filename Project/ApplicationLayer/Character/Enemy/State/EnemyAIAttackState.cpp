#include "EnemyAIAttackState.h"
#include "Enemy.h"
#include "EnemyAIWanderState.h"
#include "EnemyAIChaseState.h"

#include "Player.h"
#include "LevelObjectManager.h"

namespace K4E = ::Ken4lowEngine;

void EnemyAIAttackState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;

	// ステートを攻撃に変更
	enemy->SetState(AIState::Attack);

	// ★攻撃ステートに入った瞬間にタイマーとフラグをリセット
	enemy->SetStateTimer(0.0f);
	Enemy::AttackInfo& attack = enemy->GetAttackInfo();
	attack.didHitThisAttack = false;
}

void EnemyAIAttackState::Update(Enemy* enemy, float deltaTime)
{
	using AIState = Enemy::AIState;
	using AttackInfo = Enemy::AttackInfo;
	using WanderInfo = Enemy::WanderInfo;

	AIState stateId = enemy->GetCurrentState();

	// エイリアス
	AttackInfo& attack = enemy->GetAttackInfo();
	WanderInfo& wander = enemy->GetWanderInfo();
	auto& body = enemy->GetBody();
	auto& parts = enemy->GetBodyParts();
	auto& partIndices = enemy->GetPartIndices();
	Player* player = enemy->GetPlayer();

	float stateTimer = enemy->GetStateTimer();
	float raiseAngleDeg = enemy->GetRaiseAngleDeg();   // 腕を上げるときの角度
	float hitAngleDeg = enemy->GetHitAngleDeg();       // ヒット時の腕の角度
	float returnAngleDeg = enemy->GetReturnAngleDeg(); // 腕を戻すときの角度

	// 攻撃クールダウンタイマーを進める
	if (attack.cooldownTimer > 0.0f)
	{
		attack.cooldownTimer -= deltaTime;
		if (attack.cooldownTimer < 0.0f) attack.cooldownTimer = 0.0f;
	}

	// プレイヤーが死んでいたら追跡・攻撃は中断して徘徊に戻す
	if (player && player->IsDeadNow())
	{
		if (stateId == AIState::Chase || stateId == AIState::Attack)
		{
			attack.didHitThisAttack = false; // 念のため攻撃フラグもリセット
			enemy->SetStateTimer(0.0f);
			enemy->SetState(AIState::Wander);
			enemy->ChangeState(std::make_unique<EnemyAIWanderState>()); // 徘徊状態へ
		}
	}

	// いまの位置を覚えておく（衝突解決の基準になる）
	K4E::Vector3 oldPos = body.transform.translate_;

	if (!player)
	{
		enemy->SetState(AIState::Wander);
		enemy->ChangeState(std::make_unique<EnemyAIWanderState>()); // 徘徊状態へ
		return;
	}

	// プレイヤーが死んでいたら徘徊に戻る
	if (player->IsDeadNow())
	{
		attack.didHitThisAttack = false;
		enemy->SetStateTimer(0.0f);
		enemy->SetState(AIState::Wander);
		enemy->ChangeState(std::make_unique<EnemyAIWanderState>()); // 徘徊状態へ
		return;
	}

	// 距離と向きは今までどおり
	K4E::Vector3 to = player->GetCenterPosition() - body.transform.translate_;
	to.y = 0.0f;
	float dist = K4E::Vector3::Length(to);
	if (dist < 0.0001f) dist = 0.0001f;
	K4E::Vector3 dir = to / dist;

	// 顔はターゲットを見る（いまのコードと同じ）
	body.transform.rotate_.y = std::atan2f(-dir.x, dir.z);

	// ※ここで "押し戻り" はあえてほぼしない（後で詳しく話す）
	// ただしめり込みがエグいほど近いときだけ、ほんの少しだけ離すのはアリ
	const float minDist = enemy->GetPersonalSpaceRadius();
	const float keep = minDist * 0.6f;
	if (dist < keep) {
		body.transform.translate_ -= dir * (keep - dist);
		dist = keep; // 計算をこの距離で続行
	}

	// 攻撃タイマーを進める
	stateTimer += deltaTime;
	enemy->SetStateTimer(stateTimer);

	// 便宜上ローカル変数にコピー
	float t = stateTimer;

	// -------- アニメ用のフェーズ進捗を計算 --------
	// 0→1で腕を上げる
	float up01 = std::clamp(t / attack.windup, 0.0f, 1.0f);
	// 0→1で腕を振り下ろす（windup後からカウント）
	float swing01 = std::clamp((t - attack.windup) / attack.swing, 0.0f, 1.0f);
	// 0→1で腕を戻す（windup+swing後からカウント）
	float rec01 = std::clamp(
		(t - attack.windup - attack.swing) / attack.recover,
		0.0f, 1.0f
	);

	// 腕の角度（シンプルにx回転だけでOK）
	auto deg2rad = [](float d) { return d * std::numbers::pi_v<float> / 180.0f; };

	// 基本は腕を前に振り上げるように -70度くらい
	float raisedAngle = deg2rad(raiseAngleDeg);

	float armAngleRad = 0.0f;
	if (t < attack.windup)
	{
		// 溜め: 徐々に腕を上げる
		armAngleRad = raisedAngle * up01;
	}
	else if (t < attack.windup + attack.swing)
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
		float toDeg = wander.idlePoseAngleDeg;    // 例: -70.0f ← 初期ポーズと同じ角度
		float curDeg = std::lerp(fromDeg, toDeg, rec01);

		armAngleRad = curDeg * std::numbers::pi_v<float> / 180.0f;
	}

	// 両腕を回転させる（parts_[leftArmIndex_] / parts_[rightArmIndex_]）
	if (partIndices.leftArm < parts.size() && parts[partIndices.leftArm].object)
	{
		parts[partIndices.leftArm].transform.rotate_.x = armAngleRad;
	}
	if (partIndices.rightArm < parts.size() && parts[partIndices.rightArm].object)
	{
		parts[partIndices.rightArm].transform.rotate_.x = armAngleRad;
	}

	// -------- 実ダメージとノックバック --------
	// 今のコードは「UpdateAttack()が始まった瞬間に毎回 player_->TakeDamage() を呼んでる」ので、
	// 見た目と当たりのタイミングがズレてる
	//
	// swingフェーズが始まった瞬間（=腕を振り下ろし始めた瞬間）に1回だけヒットさせる
	if (!attack.didHitThisAttack && t >= attack.windup && t < attack.windup + attack.swing)
	{
		player->ApplyDamageImpulse(dir, attack.power, 0.15f);
		player->TakeDamage(attack.damage);

		attack.didHitThisAttack = true;
		attack.cooldownTimer = attack.cooldown; // クールダウン開始
	}

	// -------- 攻撃が終わったらChaseに戻る --------
	float totalTime = attack.windup + attack.swing + attack.recover;
	if (t >= totalTime)
	{
		// 腕をアイドル角度に戻す
		float idleRad = wander.idlePoseAngleDeg * std::numbers::pi_v<float> / 180.0f;
		if (partIndices.leftArm < parts.size() && parts[partIndices.leftArm].object) {
			parts[partIndices.leftArm].transform.rotate_.x = idleRad;
		}
		if (partIndices.rightArm < parts.size() && parts[partIndices.rightArm].object) {
			parts[partIndices.rightArm].transform.rotate_.x = idleRad;
		}

		// 次の状態へ
		enemy->SetStateTimer(0.0f);
		enemy->SetState(AIState::Chase);
		enemy->ChangeState(std::make_unique<EnemyAIChaseState>()); // 徘徊状態へ
	}

	enemy->SetStateTimer(stateTimer);
}

void EnemyAIAttackState::Exit(Enemy* enemy)
{
	(void)enemy;
}
