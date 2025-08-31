#define NOMINMAX
#include "Enemy.h"
#include "Player.h"
#include <cmath> // atan2f
#include <ScoreManager.h>
#include <algorithm>

std::vector<Enemy*> Enemy::sActives;

Enemy::~Enemy()
{
	// 破棄時にアクティブリストから削除
	sActives.erase(std::remove(sActives.begin(), sActives.end(), this), sActives.end());
}

void Enemy::Initialize(Player* player, const Vector3& position)
{
	player_ = player;
	hp_ = maxHp_;
	isDead_ = false;
	// speed_ はヘッダ既定の 5.5f を使う（Initialize で 0.1f にしない）

	attackRange_ = 2.0f;
	attackCooldown_ = 1.0f;
	attackTimer_ = 0.0f;
	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));

	model_ = std::make_unique<Object3D>();
	model_->Initialize("cube.gltf");
	model_->SetTranslate(position);

	// コライダーの初期化（OBB）
	SetOBBHalfSize({ 1.0f, 1.0f, 1.0f });
	SetOwner(this);
	SetCenterPosition(position);
	sActives.push_back(this);
}

void Enemy::Update()
{
	if (isDead_ || !player_) return;

	const float dt = player_->GetDeltaTime();
	Vector3 pos = model_->GetTranslate();

	// ノックバック速度の適用（既存）
	pos += knockVel_ * dt;
	knockVel_ *= 0.82f;

	// Y=0 でクランプ（地形APIがあれば置き換え）
	if (pos.y < 0.0f) pos.y = 0.0f;

	// 追跡（既存）
	Vector3 toP = PlayerWorldPosition() - pos;
	Vector3 flat = { toP.x, 0.0f, toP.z };
	float dist = Vector3::Length(flat);
	if (dist > 1e-4f) {
		flat = flat / dist;
		if (dist > attackRange_) pos += flat * (speed_ * dt);
		float yaw = std::atan2f(flat.x, flat.z);
		model_->SetRotate({ 0.0f, yaw, 0.0f });
	}

	// ★ 近すぎる仲間から“水平に”押し広げる
	SeparateFromNeighbors(pos, dt);

	model_->SetTranslate(pos);
	model_->Update();
	SetCenterPosition(pos);
}

void Enemy::Draw()
{
	if (isDead_ || !model_) return;
	model_->Draw();
}

void Enemy::OnCollision(Collider* other)
{
	if (isDead_) return;
	if (!other) return;
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		// 近接DPSダメージ
		ApplyMeleeDamage();

		// 軽いノックバック
		ApplyKnockback(PlayerWorldPosition() - model_->GetTranslate(), knockback_);
	}
}

void Enemy::TakeDamage(int damage)
{
	if (isDead_) return;
	hp_ -= damage;
	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;
		isDead_ = true;

		// ★死亡が確定した瞬間に一度だけキル加算
		ScoreManager::GetInstance()->AddKill();
	}
}

Vector3 Enemy::PlayerWorldPosition()
{
	if (!player_) return Vector3{};            // ★ 必ず戻り値を返す
	auto* model = player_->GetAnimationModel();
	return model ? model->GetTranslate() : Vector3{};
}

void Enemy::ApplyMeleeDamage()
{
	if (!player_) return;
	const float dt = player_->GetDeltaTime();  // 実dtでDPS換算
	player_->TakeDamage(dps_ * dt);
}

void Enemy::SeparateFromNeighbors(Vector3& currentPos, float dt)
{
	const float selfR = avoidRadius_;
	const float maxPushPerSec = 6.0f;          // 1秒あたり最大押し戻し量（調整用）
	const float eps = 1e-4f;

	Vector3 accum{}; // 積算補正

	for (Enemy* e : sActives) {
		if (e == this || e->isDead_) continue;
		Vector3 op = e->model_->GetTranslate();   // 相手の位置
		Vector3 d = currentPos - op; d.y = 0.0f;         // 水平だけを見る
		float L = Vector3::Length(d);
		const float minDist = selfR + e->avoidRadius_; // 望ましい最小距離

		if (L > eps && L < minDist) {
			Vector3 n = d / L;                    // 押し戻し方向
			float overlap = (minDist - L);        // 重なり量
			accum += n * overlap;                 // まず合力で貯める
		}
	}

	// 積算補正を時間でスケール＆クランプして適用
	float len = Vector3::Length(accum);
	if (len > eps) {
		Vector3 corr = (accum / len) * std::min(len, maxPushPerSec * dt);
		currentPos += corr;
		// 地面クランプ維持
		if (currentPos.y < 0.0f) currentPos.y = 0.0f;
	}
}

void Enemy::ApplyKnockback(const Vector3& direction, float power)
{
	if (isDead_) return;

	// 水平成分だけでノックバック（地面に押し込まない）
	Vector3 n = direction;
	n.y = 0.0f;
	float len = Vector3::Length(n);
	if (len > 1e-5f) n = n / len; else n = { 0,0,0 };

	// 0.1秒くらい効くインパルスに換算して速度に足す
	const float impulse = power / 0.1f;
	knockVel_ += n * impulse;
}
