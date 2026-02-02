#include "Item.h"
#include "Player.h"
#include "ScoreManager.h"
#include "GpuParticleEmitter.h"   // EmitterInfo を使うため（必要なら）
#include "GpuParticleType.h"      // GpuParticleType::Heal_Effect
#include "BillboardMode.h"
#include <CollisionTypeIdDef.h>

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Item::Initialize(ItemType type, const Vector3& pos)
{
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kItem));
	Collider::SetOBBHalfSize(scale_);

	// アイテムの種類と位置を設定
	type_ = type;
	position_ = pos;
	basePosition_ = pos;

	std::string modelPath; // モデルパス
	object3d_ = std::make_unique<Object3D>();

	// アイテムの種類に応じてモデルと色を設定
	switch (type_)
	{
	case ItemType::HealSmall:
		modelPath = "cube.gltf"; break;

	case ItemType::AmmoSmall:
		modelPath = "cube.gltf"; break;

	case ItemType::ScoreBonus:
		modelPath = "cube.gltf"; break;

	case ItemType::PowerUp:
		modelPath = "cube.gltf"; break;

	case ItemType::ExperienceOrb:
		modelPath = "cube.gltf"; break;

	case ItemType::Coin:
		modelPath = "cube.gltf"; break;

	case ItemType::NextStageKey:
		modelPath = "cube.gltf"; break;
	}

	object3d_->Initialize(modelPath);
	object3d_->SetTranslate(position_);
	object3d_->SetScale(scale_); // サイズを設定

	// 条件によって色を変える（変更予定）
	switch (type_)
	{
	case ItemType::HealSmall:
		object3d_->SetColor({ 1.0f,0.0f,0.0f,1.0f }); // 赤色
		break;

	case ItemType::AmmoSmall:
		object3d_->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f }); // 青色
		break;

	case ItemType::ScoreBonus:
		object3d_->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 黄色
		break;

	case ItemType::PowerUp:
		object3d_->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f }); // オレンジ色
		break;

	case ItemType::ExperienceOrb:
		object3d_->SetColor({ 0.5f, 0.0f, 0.5f, 1.0f }); // 紫色
		break;

	case ItemType::Coin:
		object3d_->SetColor({ 1.0f, 0.84f, 0.0f, 1.0f }); // 金色
		break;

	case ItemType::NextStageKey:
		object3d_->SetColor({ 0.0f, 1.0f, 1.0f, 1.0f }); // シアン色
		break;
	}
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Item::Update(float deltaTime)
{
	// 収集済みなら更新しない
	if (collected_) return;

	// ライフタイム更新
	lifetime_ += deltaTime;

	// 浮遊アニメーション：サイン波でY位置を変更
	floatTimer_ += floatSpeed_ * deltaTime;  // フレーム前提。可変FPSなら deltaTime を使う
	float floatOffset = std::sinf(floatTimer_) * floatAmplitude_;
	position_.y = basePosition_.y + floatOffset + 1.0f;

	// Y軸を中心に回転
	rotation_.y += rotationSpeed_;

	object3d_->SetTranslate(position_);
	object3d_->SetRotate(rotation_);
	object3d_->Update();

	Collider::SetCenterPosition(position_);
}

/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Item::Draw()
{
	if (!collected_ && object3d_) {
		object3d_->Draw();
	}
}

/// -------------------------------------------------------------
///						プレイヤーとの当たり判定
/// -------------------------------------------------------------
bool Item::CheckCollisionWithPlayer(const Vector3& playerPos)
{
	const float pickupRadius = 2.0f;
	const Vector3 diff = position_ - playerPos;
	// 距離^2 ≤ 半径^2
	return Vector3::Length(diff) <= (pickupRadius * pickupRadius);
}

/// -------------------------------------------------------------
///						効果適用
/// -------------------------------------------------------------
void Item::ApplyTo(Player* player)
{
	if (collected_ || !player) return;

	switch (type_)
	{
	case ItemType::HealSmall:
	{
		// ① 回復（値は仮）
		//player->AddHP(300);

		// ② 回復パーティクル（下→上の Emit はシェーダ側 type=21 で実装済み想定）
		////auto* pm = GpuParticleManager::GetInstance();

		//// エミッターを作ってなければ作る（1回だけ）
		//GpuParticleEmitter* emitter = pm->GetEmitter("Heal_Effect");
		//if (!emitter)
		//{
		//	GpuParticleEmitter::EmitterInfo info{};
		//	info.textureFilePath = "white.png"; // とりあえず既存の白テクでOK（後で差し替え）
		//	info.radius = 1.5f;                // 正方形範囲の“半辺”として使う想定
		//	info.loopCount = 0;
		//	info.loopFrequency = 0.0f;
		//	info.drawType = 0;                 // 0なら type を使う
		//	info.type = GpuParticleType::Heal_Effect;
		//	info.billboardMode = BillboardMode::Camera;

		//	emitter = pm->CreateEmitter("Heal_Effect", info);
		//}

		//if (emitter)
		//{
		//	// プレイヤー位置にセットしてバースト
		//	// ※ Player の位置取得はあなたの実装に合わせて差し替え
		//	emitter->SetPosition(player->GetCenterPosition() - Vector3(0.0f, 2.0f, 0.0f));
		//	emitter->RequestEmit(40); // まずは 30〜60 で調整
		//}
		break;
	}

	// 他のアイテムもここに追加していける
	default:
		break;
	}

	collected_ = true;
}

/// -------------------------------------------------------------
///						衝突時の処理
/// -------------------------------------------------------------
void Item::OnCollision(Collider* other)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		ApplyTo(static_cast<Player*>(other)); // 効果適用
	}
}
