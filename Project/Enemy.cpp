#include "Enemy.h"


/// -------------------------------------------------------------
///							　初期化処理
/// -------------------------------------------------------------
void Enemy::Initialize()
{
	// 基底クラスの初期化処理
	BaseCharacter::Initialize();

	// 体（親）の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Enemy/enemy.gltf");
	body_.transform.Initialize();
	body_.transform.translate_ = { radius_, 0, 0 };

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
		part.transform.translate_ = position;
		part.object->SetTranslate(part.transform.translate_);
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

	// 円運動を更新
	angle_ += speed_;
	if (angle_ > 2.0f * 3.14159265f) // 角度のリセット
	{
		angle_ -= 2.0f * 3.14159265f;
	}

	// 新しい位置を計算
	float x = radius_ * cos(angle_);
	float z = radius_ * sin(angle_);

	// 移動方向のベクトル
	float velocityX = x - body_.transform.translate_.x;
	float velocityZ = z - body_.transform.translate_.z;

	// 向きの計算（atan2を使用）
	float targetAngle = atan2(-velocityX, velocityZ) * (180.0f / 3.14159265f);
	float smoothAngle = Vector3::AngleLerp(body_.transform.rotate_.y * (180.0f / 3.14159265f), targetAngle, 0.2f);
	body_.transform.rotate_.y = smoothAngle * (3.14159265f / 180.0f);

	// 位置を更新
	body_.transform.translate_ = { x, 0, z };
}


/// -------------------------------------------------------------
///							　描画処理
/// -------------------------------------------------------------
void Enemy::Draw()
{
	// 基底クラスの描画処理
	BaseCharacter::Draw();
}
