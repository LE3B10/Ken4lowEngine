#include "Enemy.h"
#include "DirectXCommon.h"
#include "Hammer.h"
#include <CollisionTypeIdDef.h>
#include "SceneManager.h"
#include "Player.h"


Enemy::Enemy()
{
	// シリアルナンバーを振る
	serialNumber_ = nextSerialNumber_;
	// 次のシリアルナンバーに1を足す
	++nextSerialNumber_;
}


/// -------------------------------------------------------------
///							　初期化処理
/// -------------------------------------------------------------
void Enemy::Initialize()
{
	// 基底クラスの初期化処理
	BaseCharacter::Initialize();
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy));

	dxCommon_ = DirectXCommon::GetInstance();

	// 体（親）の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Enemy/enemy.gltf");
	body_.transform.Initialize();
	body_.transform.translation_ = { radius_, 0, 0 };

	// 子オブジェクト（頭、腕）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"Enemy/enemy_LArm.gltf", {0.0f, 2.0f, 0.0f}}, // 左腕
		{"Enemy/enemy_RArm.gltf", {0.0f, 2.0f, 0.0f}}  // 右腕
	};

	for (const auto& [modelPath, position] : partData)
	{
		BodyPart part;
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.transform.Initialize();
		part.transform.translation_ = position;
		part.object->SetTranslate(part.transform.translation_);
		part.transform.parent_ = &body_.transform; // 親を設定
		parts_.push_back(std::move(part));
	}
}


/// -------------------------------------------------------------
///							　更新処理
/// -------------------------------------------------------------
void Enemy::Update()
{
	// 基底クラスの更新処理
	BaseCharacter::Update();

	// 無敵時間のカウントダウン
	if (isInvincible_)
	{
		invincibleTimer_ -= 1.0f / dxCommon_->GetFPSCounter().GetFPS(); // 60FPS基準で計算
		if (invincibleTimer_ <= 0.0f)
		{
			isInvincible_ = false; // 無敵終了
		}
	}

	// 円運動を更新
	angle_ += speed_;
	if (angle_ > 2.0f * std::numbers::pi_v<float>) // 角度のリセット
	{
		angle_ -= 2.0f * std::numbers::pi_v<float>;
	}

	// 新しい位置を計算
	float x = center_.x + radius_ * cos(angle_);
	float z = center_.y + radius_ * sin(angle_);

	// 移動方向のベクトル
	float velocityX = x - body_.transform.translation_.x;
	float velocityZ = z - body_.transform.translation_.z;

	// 向きの計算（atan2を使用）
	float targetAngle = atan2(-velocityX, velocityZ) * (180.0f / std::numbers::pi_v<float>);
	float smoothAngle = Vector3::AngleLerp(body_.transform.rotate_.y * (180.0f / std::numbers::pi_v<float>), targetAngle, 0.2f);
	body_.transform.rotate_.y = smoothAngle * (std::numbers::pi_v<float> / 180.0f);

	// 位置を更新
	body_.transform.translation_ = { x, 0, z };

	// 腕のアニメーション更新
	UpdateArmAnimation();
}


/// -------------------------------------------------------------
///							　描画処理
/// -------------------------------------------------------------
void Enemy::Draw()
{
	if (!isDead_)
	{
		// 通常描画
		if (!isInvincible_ || static_cast<int>(invincibleTimer_ * 10) % 2 == 0)
		{
			// 基底クラスの描画処理
			BaseCharacter::Draw();
		}
	}
}

void Enemy::OnCollision(Collider* other)
{
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();
	OutputDebugStringA("Enemy OnCollision called.\n");

	// 衝突相手が武器なら
	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		Player* player = static_cast<Player*>(other);
		uint32_t serialNumber = player->GetSerialNumber();

		// 接触記録があれば何もせず抜ける
		if (contactRecord_.Check(serialNumber)) return;

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		OutputDebugStringA("Collision with Player detected.\n");
	}
}

Vector3 Enemy::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,1.8f,0.0f };
	Vector3 worldPosition = body_.transform.translation_ + offset;
	return worldPosition;
}


void Enemy::SetDamage(int damage)
{
	if (!isInvincible_)
	{
		enemyHP_ -= damage;
		if (enemyHP_ <= 0)
		{
			isDead_ = true;
		}
		isInvincible_ = true;           // 無敵フラグをON
		invincibleTimer_ = invincibleDuration_; // 無敵時間をリセット
	}
}

/// -------------------------------------------------------------
///						腕のアニメーション更新
/// -------------------------------------------------------------
void Enemy::UpdateArmAnimation()
{
	// アニメーションパラメータを更新
	armSwingParameter_ += kArmSwingSpeed;

	// 2πを超えたらリセット（ループ）
	armSwingParameter_ = std::fmod(armSwingParameter_, 2.0f * std::numbers::pi_v<float>);

	// 腕の振り角度をサイン波で計算
	float targetSwingAngle = kMaxArmSwingAngle * sinf(armSwingParameter_);

	// 腕のアニメーションを適用
	if (parts_.size() >= 2) // 腕のデータが存在するか確認
	{
		parts_[0].transform.rotate_.x = -targetSwingAngle; // 左腕
		parts_[1].transform.rotate_.x = targetSwingAngle;  // 右腕
	}
}
