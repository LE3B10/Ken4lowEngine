#include "EnemyAIDamagedState.h"
#include "Enemy.h"
#include "LinearInterpolation.h"
#include <EnemyAIChaseState.h>
#include <EnemyAIDeadState.h>
#include <Player.h>

void EnemyAIDamagedState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;
	using FlashInfo = Enemy::FlashInfo;

	// 初期状態はDamagedに設定
	enemy->SetState(AIState::Damaged);

	FlashInfo& flash = enemy->GetFlashInfo();
	Player* player = enemy->GetPlayer();

	// 1. フラッシュの初期化
	//flash.timer = flash.duration;     // 残り時間リセット
	flash.colorModulate = flash.hitColor;    // まずは真っ赤に

	// すぐに赤色を適用（1フレーム目から赤く見せたい）
	ApplyColorToAll(enemy, flash.colorModulate);

	// 2. HPを1回だけ減らす（OnCollisionでは減らさない）
	if (player)
	{
		float hp = enemy->GetHp();
		hp -= player->GetWeaponManager()->GetCurrentConfig().damage;
		enemy->SetHp(hp);
	}
}

void EnemyAIDamagedState::Update(Enemy* enemy, float deltaTime)
{
	using AIState = Enemy::AIState;
	using FlashInfo = Enemy::FlashInfo;

	FlashInfo& flash = enemy->GetFlashInfo();

	// 1. フラッシュ演出
	if (flash.timer > 0.0f)
	{
		flash.timer -= deltaTime;
		if (flash.timer < 0.0f) flash.timer = 0.0f;

		float t = 1.0f - std::clamp(flash.timer / flash.duration, 0.0f, 1.0f); // 0→1
		Vector4 c = {
			Lerp(flash.hitColor.x,  flash.baseColor.x,  t),
			Lerp(flash.hitColor.y,  flash.baseColor.y,  t),
			Lerp(flash.hitColor.z,  flash.baseColor.z,  t),
			Lerp(flash.hitColor.w,  flash.baseColor.w,  t),
		};
		ApplyColorToAll(enemy, c);

		// まだフラッシュ中 → ここで終わり
		return;
	}
	else
	{
		// 念のため最後に元の色に戻す
		ApplyColorToAll(enemy, flash.baseColor);
	}

	// 2. フラッシュが終わったタイミングでステート遷移だけ行う

	float hp = enemy->GetHp();

	if (hp <= 0.0f)
	{
		// 死亡へ
		enemy->SetHp(0.0f);
		StartDeathSequence(enemy);
		enemy->SetState(AIState::Dead);
		enemy->ChangeState(std::make_unique<EnemyAIDeadState>());
		return;
	}

	// 生きているなら追跡状態へ戻す
	enemy->ResetStateTimer();
	enemy->SetState(AIState::Chase);
	enemy->ChangeState(std::make_unique<EnemyAIChaseState>());
}

void EnemyAIDamagedState::Exit(Enemy* enemy)
{
	(void)enemy; // 未使用
}

void EnemyAIDamagedState::StartDeathSequence(Enemy* enemy)
{
	using AIState = Enemy::AIState;
	using DeathEnemyState = Enemy::DeathEnemyState;
	using GibMotion = Enemy::GibMotion;
	using BodyPart = BaseCharacter::BodyPart;

	DeathEnemyState& death = enemy->GetDeathState();
	BodyPart& body = enemy->GetBody();
	std::vector<BodyPart>& parts = enemy->GetBodyParts();
	Vector3& dropPosAtDeath = enemy->GetDropPosAtDeath();

	if (death.active) return;

	dropPosAtDeath = enemy->GetCenterPosition(); // 死亡時の位置を保存

	death.active = true;
	death.finished = false;
	death.timer = 0.0f;
	death.duration = 0.8f;

	enemy->SetActive(true); // 死亡演出中はアクティブにする

	death.gibs.clear();
	death.gibs.resize(parts.size());

	static thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
	std::uniform_real_distribution<float> upDist(2.0f, 6.0f);
	std::uniform_real_distribution<float> angDist(-6.0f, 6.0f);

	for (size_t i = 0; i < parts.size(); ++i)
	{
		auto& part = parts[i];
		GibMotion gm{};

		// ランダム速度
		Vector3 dir{ dirDist(rng), 0.0f, dirDist(rng) };
		if (Vector3::Length(dir) < 0.001f) dir = { 0.0f, 0.0f, 1.0f };
		dir = Vector3::Normalize(dir);
		gm.velocity = dir * 3.0f;
		gm.velocity.y += upDist(rng);
		gm.angularVelocity = { angDist(rng), angDist(rng), angDist(rng) };
		death.gibs[i] = gm;

		// 今までの translate_ は body からのローカル位置なので、
		// 親を外す前に「ワールド位置」に焼き直す
		Vector3 worldPos = part.transform.translate_;
		if (part.transform.parent_ == &body.transform) {
			worldPos += body.transform.translate_;
		}

		part.transform.parent_ = nullptr;
		part.transform.translate_ = worldPos;
	}
}

void EnemyAIDamagedState::ApplyColorToAll(Enemy* enemy, const Vector4& color)
{
	using FlashInfo = Enemy::FlashInfo;
	using BodyPart = BaseCharacter::BodyPart;
	FlashInfo& flash = enemy->GetFlashInfo();
	BodyPart& body = enemy->GetBody();
	std::vector<BodyPart>& parts = enemy->GetBodyParts();

	// 全パーツに色を適用
	flash.colorModulate = color;

	// 本体
	if (body.object) { body.object->SetColor(flash.colorModulate); }

	// 部位群
	for (auto& part : parts) {
		if (part.object) { part.object->SetColor(flash.colorModulate); }
	}
}
