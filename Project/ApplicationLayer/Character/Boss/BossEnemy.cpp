#include "BossEnemy.h"
#include <CollisionTypeIdDef.h>

/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void BossEnemy::Initialize()
{
	// ベースキャラクター初期化
	BaseCharacter::Initialize();

	// テクスチャの設定
	BaseCharacter::ApplySkinToAllParts(skinTexturePath_);

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBoss));
	Collider::SetOwner<BossEnemy>(this); //	オーナー設定
	Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f });
}

/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void BossEnemy::Update(float deltaTime)
{
	Collider::SetCenterPosition(GetCenterPosition());
	// ベースキャラクター更新
	BaseCharacter::Update(deltaTime);
}

/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void BossEnemy::Draw()
{
	BaseCharacter::Draw();
}

/// -------------------------------------------------------------
///					　		ImGui描画処理
/// -------------------------------------------------------------
void BossEnemy::DrawImGui()
{
#ifdef USE_IMGUI

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///					　		衝突判定処理
/// -------------------------------------------------------------
void BossEnemy::OnCollision(Collider* other)
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
		OutputDebugStringA("Boss hit by bullet!\n");
	}
}

/// -------------------------------------------------------------
///					　		中心座標取得
/// -------------------------------------------------------------
Vector3 BossEnemy::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,0.0f,0.0f };
	return body_.transform.translate_ + offset;
}
