#include "Hammer.h"
#include "CollisionTypeIdDef.h"
#include "TextureManager.h"
#include "Player.h"


Hammer::Hammer()
{
	// シリアルナンバーを振る
	serialNumber_ = nextSerialNumber_;
	// 次のシリアルナンバーに1を足す
	++nextSerialNumber_;
}

/// -------------------------------------------------------------
///						　	初期化処理
/// -------------------------------------------------------------
void Hammer::Initialize()
{
	Collider::Initialize();
	Collider::SetRadius(2.0f);
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kWeapon));

	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");

	enemy_ = std::make_unique<Enemy>();

	object_ = std::make_unique<Object3D>();
	object_->Initialize("Hammer/Hammer.gltf"); // ハンマーの3Dモデル
	worldTransform_.Initialize();
	worldTransform_.translation_ = { 0.0f, 4.5f, 0.0f }; // 初期位置（手元）

	particleManager_ = ParticleManager::GetInstance();
	particleManager_->CreateParticleGroup("Hammer", "Resources/uvChecker.png");

	particleEmitter_ = std::make_unique<ParticleEmitter>(particleManager_, "Hammer");
}


/// -------------------------------------------------------------
///						　	更新処理
/// -------------------------------------------------------------
void Hammer::Update()
{
	if (parentTransform_)
	{
		worldTransform_.translation_ = parentTransform_->translation_ + offset_; // 親の座標に同期

		// 親（プレイヤー）の回転を取得
		worldTransform_.rotate_.y = parentTransform_->rotate_.y;
	}

	// TODO: プレイヤーの腕の動きに合わせる処理
	object_->SetTranslate(worldTransform_.translation_);
	object_->SetRotate(worldTransform_.rotate_);

	// 更新処理
	object_->Update();
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Hammer::Draw()
{
	object_->Draw();
}


/// -------------------------------------------------------------
///						　接触履歴を削除
/// -------------------------------------------------------------
void Hammer::ClearContactRecord()
{
	contactRecord_.Clear();
}


/// -------------------------------------------------------------
///						　衝突時の反応処理
/// -------------------------------------------------------------
void Hammer::OnCollision(Collider* other)
{
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();
	OutputDebugStringA("Hammer OnCollision called.\n");

	// 衝突相手が敵なら
	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		Enemy* enemy = static_cast<Enemy*>(other);
		uint32_t serialNumber = enemy->GetSerialNumber();

		// 接触記録があれば何もせず抜ける
		if (contactRecord_.Check(serialNumber)) return;

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		OutputDebugStringA("Collision with Enemy detected.\n");

		float deltaTime = 1.0f / 30.0f;
		// 敵の位置にエフェクトを発生
		particleEmitter_->SetPosition({ transformedPosition.x, transformedPosition.y, transformedPosition.z });
		particleEmitter_->SetEmissionRate(1000.0f);
		particleEmitter_->Update(deltaTime);
	}
}


/// -------------------------------------------------------------
///						　中心座標を取得
/// -------------------------------------------------------------
Vector3 Hammer::GetCenterPosition() const
{
	// 行列を最新に更新
	const_cast<WorldTransform&>(worldTransform_).Update();

	const Vector4 offset = { 0.0f, 12.0f, 2.0f, 1.0f };
	transformedPosition = Vector4::Transform(offset, worldTransform_.matWorld_);

	// wで正規化
	if (transformedPosition.w != 0.0f) {
		transformedPosition.x /= transformedPosition.w;
		transformedPosition.y /= transformedPosition.w;
		transformedPosition.z /= transformedPosition.w;
	}

	return { transformedPosition.x, transformedPosition.y, transformedPosition.z };
}
