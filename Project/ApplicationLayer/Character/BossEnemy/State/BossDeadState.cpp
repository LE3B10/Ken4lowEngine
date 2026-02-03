#include "BossDeadState.h"
#include "BossEnemy.h"
#include "BossEnemyVfx.h"

#include <random>

namespace K4E = ::Ken4lowEngine;

void BossDeadState::OnEnter(BossEnemy* boss)
{
	using GibMotion = BossEnemy::GibMotion;
	auto& death_ = boss->GetDeathState();
	auto& parts_ = boss->GetBodyParts();
	auto& body_ = boss->GetBody();

	// nullチェック
	if (!boss) return;

	// 二重開始防止
	if (death_.active || death_.finished) return;

	death_.active = true;
	death_.finished = false;
	death_.timer = 0.0f;
	death_.startBurstDone = false;

	// 簡単な吹っ飛び設定（体幹）
	death_.bodyGib.velocity = { 0.0f, 8.0f, 0.0f };
	death_.bodyGib.angularVelocity = { 0.0f, 5.0f, 0.0f };

	death_.gibs.clear();
	death_.gibs.resize(parts_.size());

	static thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
	std::uniform_real_distribution<float> upDist(2.0f, 6.0f);
	std::uniform_real_distribution<float> angDist(-6.0f, 6.0f);

	for (size_t i = 0; i < parts_.size(); ++i)
	{
		auto& part = parts_[i];
		GibMotion gm{};

		K4E::Vector3 dir{ dirDist(rng), 0.0f, dirDist(rng) };
		if (K4E::Vector3::Length(dir) < 0.001f) dir = { 0.0f, 0.0f, 1.0f };
		dir = K4E::Vector3::Normalize(dir);

		gm.velocity = dir * 3.0f;
		gm.velocity.y += upDist(rng);
		gm.angularVelocity = { angDist(rng), angDist(rng), angDist(rng) };
		death_.gibs[i] = gm;

		// ローカル→ワールド焼き直しして親を外す（既存ロジックそのまま）
		K4E::Vector3 worldPos = part.transform.translate_;
		if (part.transform.parent_ == &body_.transform) {
			worldPos += body_.transform.translate_;
		}

		part.transform.parent_ = nullptr;
		part.transform.translate_ = worldPos;
	}
}

BehaviorStatus BossDeadState::Update(BossEnemy* boss, float deltaTime)
{
	if (!boss) return BehaviorStatus::Failure;

	using GibMotion = BossEnemy::GibMotion;
	auto& death_ = boss->GetDeathState();
	auto& parts_ = boss->GetBodyParts();
	auto& body_ = boss->GetBody();
	auto* vfx_ = boss->GetVfx();

	death_.timer += deltaTime;
	const K4E::Vector3 gravity = { 0.0f, -9.8f * 3.0f, 0.0f };
	const float   groundY = 0.5f; // 床の高さとして扱う

	// 床で止めるためのラムダ
	auto clampToGround = [&](K4E::WorldTransformEx& tr, GibMotion& gm)
		{
			if (tr.translate_.y < groundY)
			{
				tr.translate_.y = groundY;

				// 下向きの速度は殺す
				if (gm.velocity.y < 0.0f) gm.velocity.y = 0.0f;

				// すこし摩擦っぽく減速＆回転減衰
				gm.velocity.x *= 0.6f;
				gm.velocity.z *= 0.6f;
				gm.angularVelocity.x *= 0.5f;
				gm.angularVelocity.y *= 0.5f;
				gm.angularVelocity.z *= 0.5f;
			}
		};

	// body も飛ばす
	death_.bodyGib.velocity += gravity * deltaTime;
	body_.transform.translate_ += death_.bodyGib.velocity * deltaTime;
	body_.transform.rotate_ += death_.bodyGib.angularVelocity * deltaTime;
	clampToGround(body_.transform, death_.bodyGib);

	// --- パーツ ---
	for (size_t i = 0; i < parts_.size() && i < death_.gibs.size(); ++i)
	{
		auto& part = parts_[i];
		auto& gm = death_.gibs[i];

		gm.velocity += gravity * deltaTime;
		part.transform.translate_ += gm.velocity * deltaTime;
		part.transform.rotate_ += gm.angularVelocity * deltaTime;

		clampToGround(part.transform, gm);
	}

	// 終了判定
	if (death_.timer >= death_.duration)
	{
		// 一応完全に止めておく（この後は UpdateDeath 呼ばれないけど保険）
		death_.active = false;
		death_.finished = true;

		death_.bodyGib.velocity = { 0.0f, 0.0f, 0.0f };
		death_.bodyGib.angularVelocity = { 0.0f, 0.0f, 0.0f };
		for (auto& gm : death_.gibs)
		{
			gm.velocity = { 0.0f, 0.0f, 0.0f };
			gm.angularVelocity = { 0.0f, 0.0f, 0.0f };
		}
	}

	// 死亡エフェクト更新
	vfx_->UpdateDeathEffect(boss->GetCenterPosition(), death_.timer, death_.startBurstDone);

	// コライダー位置だけは同期しておく（床とのめり込み防止など）
	boss->K4E::Collider::SetCenterPosition(boss->GetCenterPosition());

	boss->BaseCharacter::Update(deltaTime);

	return death_.finished ? BehaviorStatus::Success : BehaviorStatus::Running;
}

void BossDeadState::OnExit(BossEnemy* boss)
{
	(void)boss; // 未使用
}
