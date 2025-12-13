#define NOMINMAX
#include "Boss.h"
#include <CollisionTypeIdDef.h>
#include <LinearInterpolation.h>

#include "VineSweepAttack.h"
#include "SeedMortarAttack.h"

#ifdef _DEBUG
#include <Wireframe.h>
#endif // _DEBUG

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void Boss::Initialize()
{
	BaseCharacter::Initialize();
	BaseCharacter::ApplySkinToAllParts("zombie.png");

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBoss));

	vineSweepAttack_ = std::make_unique<VineSweepAttack>();
	vineSweepAttack_->Initialize();

	seedMortarAttack_ = std::make_unique<SeedMortarAttack>();
	seedMortarAttack_->Initialize();
}

void Boss::Update(float deltaTime)
{
	// --- ボス本体の向きを取得する ---
	auto& body = GetBody();

	// まだ階層更新前なので、ここで一度だけワールド変換を更新しておく
	body.transform.Update();

	// Y軸回転を「ボスの向き」として使う（ラジアン）
	float bossYawRad = body.transform.worldRotate_.y;

	// --- プレイヤー座標取得 ---
	Vector3 playerPos{};          // TODO: 実際のプレイヤー座標を渡す

	// ツタ薙ぎ払い攻撃更新
	if (vineSweepAttack_)
	{
		vineSweepAttack_->TickCooldown(deltaTime);
		vineSweepAttack_->Update(this, deltaTime, bossYawRad, playerPos);
	}

	// 種子迫撃攻撃更新
	if (seedMortarAttack_)
	{
		seedMortarAttack_->TickCooldown(deltaTime);
		seedMortarAttack_->Update(this, deltaTime, bossYawRad, playerPos);
	}

	// 基底更新
	BaseCharacter::Update(deltaTime);
}

void Boss::Draw()
{
	BaseCharacter::Draw();

	// ツタ薙ぎ払い攻撃描画
	if (vineSweepAttack_) vineSweepAttack_->Draw();

	// 種子迫撃攻撃描画
	if (seedMortarAttack_) seedMortarAttack_->Draw();

#ifdef _DEBUG

#endif // _DEBUG

}

void Boss::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Boss Debug");

	const Vector3 c = GetCenterPosition();
	ImGui::Text("Center: (%.2f, %.2f, %.2f)", c.x, c.y, c.z);

	auto& body = GetBody();
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Body", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ローカル座標（親＝ボス本体の基準での位置・回転）を編集
		ImGui::DragFloat3("Local Position", &body.transform.translate_.x, 0.01f);
		ImGui::DragFloat3("Local Rotation", &body.transform.rotate_.x, 0.01f);
	}

	// 攻撃（ツタ薙ぎ払い）
	if (vineSweepAttack_)
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader(vineSweepAttack_->GetName(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			vineSweepAttack_->DrawImGui(*this); // ← 中身だけ描いてもらう
		}
	}

	// 攻撃（種子迫撃）
	if (seedMortarAttack_)
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader(seedMortarAttack_->GetName(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			seedMortarAttack_->DrawImGui(*this); // ← 中身だけ描いてもらう
		}
	}

	ImGui::End();
#endif // USE_IMGUI
}

void Boss::OnCollision(Collider* other)
{
	(void)other;
}

Vector3 Boss::GetCenterPosition() const
{
	return BaseCharacter::GetCenterPosition();
}
