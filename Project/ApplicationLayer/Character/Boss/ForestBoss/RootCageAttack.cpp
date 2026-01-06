#define NOMINMAX
#include "RootCageAttack.h"
#include "Boss.h"
#include "LinearInterpolation.h" // clamp01, EaseInOutCubic など
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <cmath>

namespace
{
	constexpr int   kRootCount = 12;   // 円周上の本数
	constexpr float kRootHeight = 3.0f; // ルートの高さ
}

void RootCageAttack::Initialize()
{
	rootCage_ = {};
	rootCage_.phase = Phase::Idle;
	rootCage_.cooldownTimer = 0.0f;
	rootCage_.visible = false;
	requestStart_ = false;

	columns_.clear();
	columns_.reserve(kRootCount);
	for (int i = 0; i < kRootCount; ++i)
	{
		RootColumn col;
		col.object = std::make_unique<Object3D>();
		// ★ モデルは後で差し替えOK（仮にキューブ）
		col.object->Initialize("cube.gltf");
		col.object->SetColor({ 0.4f, 0.9f, 0.4f, 1.0f });
		columns_.push_back(std::move(col));
	}
}

void RootCageAttack::TickCooldown(float deltaTime)
{
	if (rootCage_.cooldownTimer > 0.0f)
	{
		rootCage_.cooldownTimer = std::max(0.0f, rootCage_.cooldownTimer - deltaTime);
	}
}

bool RootCageAttack::CanAttack() const
{
	return (rootCage_.phase == Phase::Idle) && (rootCage_.cooldownTimer <= 0.0f);
}

void RootCageAttack::Attack()
{
	// 実際の開始は Idle フェーズで行う
	requestStart_ = true;
}

void RootCageAttack::Update(Boss* boss, float deltaTime, float bossYawRad, const Vector3& playerPosition)
{
	//const auto& p = Params(boss);

	switch (rootCage_.phase)
	{
	case Phase::Idle:
		UpdatePhase_Idle(boss, deltaTime, bossYawRad, playerPosition);
		break;
	case Phase::Windup:
		UpdatePhase_Windup(boss, deltaTime);
		break;
	case Phase::Active:
		UpdatePhase_Active(boss, deltaTime, playerPosition);
		break;
	case Phase::Recovery:
		UpdatePhase_Recovery(boss, deltaTime);
		break;
	case Phase::Cooldown:
		UpdatePhase_Cooldown(deltaTime);
		break;
	}

	// ルートの Object3D を Update
	if (rootCage_.visible)
	{
		for (auto& col : columns_)
		{
			col.object->Update();
		}
	}
}

bool RootCageAttack::IsActive() const
{
	return rootCage_.phase == Phase::Windup
		|| rootCage_.phase == Phase::Active
		|| rootCage_.phase == Phase::Recovery;
}

void RootCageAttack::Draw()
{
	if (!rootCage_.visible) return;

	for (auto& col : columns_)
	{
		col.object->Draw();
	}
}

#ifdef USE_IMGUI
void RootCageAttack::DrawImGui(Boss& boss)
{
	auto& params = boss.GetParams().rootCage;

	// フェーズ名表示用
	auto PhaseName = [](Phase phase) -> const char*
		{
			switch (phase)
			{
			case Phase::Idle:     return "Idle";
			case Phase::Windup:   return "Windup";
			case Phase::Active:   return "Active";
			case Phase::Recovery: return "Recovery";
			case Phase::Cooldown: return "Cooldown";
			default:              return "Unknown";
			}
		};

	// テスト起動 & CanAttack 表示
	ImGui::Text("Test: RootCage");
	ImGui::SameLine();
	ImGui::Text("CanAttack: %s", CanAttack() ? "YES" : "NO");

	if (ImGui::Button("Start RootCage"))
	{
		// まだ攻撃本体は未実装でも OK。後で中身を書く前提。
		Attack();
	}

	ImGui::Separator();

	// ランタイム状態表示
	ImGui::Text("Runtime");
	ImGui::Text("  phase        : %s", PhaseName(rootCage_.phase));
	ImGui::Text("  phaseTimer   : %.3f", rootCage_.phaseTimer);
	ImGui::Text("  cooldownTime : %.3f", rootCage_.cooldownTimer);
	ImGui::Text("  lockedYaw    : %.3f", rootCage_.lockedYaw);
	ImGui::Text("  visible      : %s", rootCage_.visible ? "true" : "false");
	ImGui::Text("  didHit       : %s", rootCage_.didHit ? "true" : "false");

	ImGui::Separator();

	// --------- パラメータ編集 ---------

	if (ImGui::TreeNode("Timing"))
	{
		ImGui::DragFloat("windup", &params.windup, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("growTime", &params.growTime, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("duration", &params.duration, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("cooldown", &params.cooldown, 0.01f, 0.0f, 30.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Shape"))
	{
		ImGui::DragFloat("radius", &params.radius, 0.01f, 0.1f, 50.0f);
		ImGui::DragFloat("ringThickness", &params.ringThickness, 0.001f, 0.01f, 10.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Escape"))
	{
		ImGui::DragInt("escapeDodgeCount", &params.escapeDodgeCount, 1, 0, 10);
		ImGui::DragFloat("escapeDamageToRoots", &params.escapeDamageToRoots, 1.0f, 0.0f, 10000.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Hit"))
	{
		ImGui::DragFloat("damage", &params.hit.damage, 0.5f, 0.0f, 1000.0f);
		ImGui::DragFloat("knockbackH", &params.hit.knockbackH, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("knockbackUp", &params.hit.knockbackUp, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("hitStop", &params.hit.hitStop, 0.01f, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Telegraph"))
	{
		ImGui::DragFloat("telegraph.time", &params.telegraph.time, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("telegraph.decalScale", &params.telegraph.decalScale, 0.01f, 0.1f, 5.0f);
		ImGui::TreePop();
	}

	ImGui::DragFloat("exposeCoreAfter", &params.exposeCoreAfter, 0.01f, 0.0f, 20.0f);
}
#endif // USE_IMGUI

void RootCageAttack::UpdatePhase_Idle(Boss* boss, float deltaTime, float bossYaw, const Vector3& playerPos)
{
	(void)deltaTime;

	const auto& p = Params(boss);

	// Attack() が呼ばれていてクールダウンも終わっていたら開始
	if (requestStart_ && CanAttack())
	{
		requestStart_ = false;
		rootCage_.phase = Phase::Windup;
		rootCage_.phaseTimer = 0.0f;
		rootCage_.didHit = false;
		rootCage_.visible = true;

		// プレイヤー足元あたりを中心にする
		Vector3 center = playerPos;
		center.y = boss->GetCenterPosition().y;

		BuildColumns(p, center, bossYaw);
	}
}

void RootCageAttack::UpdatePhase_Windup(Boss* boss, float deltaTime)
{
	const auto& p = Params(boss);
	rootCage_.phaseTimer += deltaTime;

	// Windup 中はテレグラフだけで、ルート自体はまだ生えない想定
	// 必要なら少しずつ生やす処理をここに追加

	if (rootCage_.phaseTimer >= p.windup)
	{
		rootCage_.phase = Phase::Active;
		rootCage_.phaseTimer = 0.0f;
	}
}

void RootCageAttack::UpdatePhase_Active(Boss* boss, float deltaTime, const Vector3& playerPos)
{
	const auto& p = Params(boss);
	rootCage_.phaseTimer += deltaTime;

	const float tGrow = rootCage_.phaseTimer / std::max(0.001f, p.growTime);
	UpdateColumnsGrow(p, tGrow);

	// 一定時間経ったら Recovery へ
	if (rootCage_.phaseTimer >= (p.growTime + p.duration))
	{
		rootCage_.phase = Phase::Recovery;
		rootCage_.phaseTimer = 0.0f;
	}

	(void)playerPos;
	// ★ 本当はここで「リング内にいるプレイヤーにヒット」などを書く
	// TryHitPlayer(boss, p, playerPos);
}

void RootCageAttack::UpdatePhase_Recovery(Boss* boss, float deltaTime)
{
	const auto& p = Params(boss);
	rootCage_.phaseTimer += deltaTime;

	const float tShrink = rootCage_.phaseTimer / std::max(0.001f, p.growTime);
	UpdateColumnsShrink(p, tShrink);

	if (rootCage_.phaseTimer >= p.growTime)
	{
		rootCage_.phase = Phase::Cooldown;
		rootCage_.phaseTimer = 0.0f;
		rootCage_.visible = false;
		rootCage_.cooldownTimer = p.cooldown;
	}
}

void RootCageAttack::UpdatePhase_Cooldown(float deltaTime)
{
	TickCooldown(deltaTime);
	if (rootCage_.cooldownTimer <= 0.0f)
	{
		rootCage_.phase = Phase::Idle;
	}
}

const ForestBossParams::RootCage& RootCageAttack::Params(const Boss* boss) const
{
	return boss->GetParams().rootCage;
}

void RootCageAttack::BuildColumns(const ForestBossParams::RootCage& p, const Vector3& center, float yaw)
{
	rootCage_.center = center;
	rootCage_.lockedYaw = yaw;

	const float radius = p.radius;
	const float angleStep = DirectX::XM_2PI / static_cast<float>(kRootCount);
	const float baseHeight = kRootHeight;

	for (int i = 0; i < kRootCount; ++i)
	{
		float angle = yaw + angleStep * static_cast<float>(i);

		Vector3 dir{ std::cos(angle), 0.0f, std::sin(angle) };
		Vector3 pos{
			center.x + dir.x * radius,
			center.y,
			center.z + dir.z * radius
		};

		auto& col = columns_[i];
		col.basePosition = pos;
		col.height = baseHeight;
		col.growthSpeed = 0.0f;

		// 最初は地面の下に潜っているイメージ
		col.object->SetScale({ p.ringThickness * 0.5f, 0.01f, p.ringThickness * 0.5f });
		col.object->SetTranslate(pos + Vector3{ 0.0f, -0.5f * baseHeight, 0.0f });
		col.object->SetRotate({ 0.0f, angle, 0.0f });
		col.object->Update();
	}
}

void RootCageAttack::UpdateColumnsGrow(const ForestBossParams::RootCage& p, float tGrow)
{
	tGrow = clamp01(tGrow);
	tGrow = EaseInOutCubic(tGrow);

	for (auto& col : columns_)
	{
		float h = col.height * tGrow;
		Vector3 scale{ p.ringThickness * 0.5f, h, p.ringThickness * 0.5f };
		Vector3 pos = col.basePosition;
		// 下端固定で上に伸びる想定（モデルの原点によって調整）
		pos.y += h * 0.5f;

		col.object->SetScale(scale);
		col.object->SetTranslate(pos);
	}
}

void RootCageAttack::UpdateColumnsShrink(const ForestBossParams::RootCage& p, float tShrink)
{
	// tShrink 0→1 でだんだん消える
	tShrink = clamp01(tShrink);
	tShrink = EaseInOutCubic(tShrink);

	float remain = 1.0f - tShrink;
	for (auto& col : columns_)
	{
		float h = col.height * remain;
		Vector3 scale{ p.ringThickness * 0.5f, h, p.ringThickness * 0.5f };
		Vector3 pos = col.basePosition;
		pos.y += h * 0.5f;

		col.object->SetScale(scale);
		col.object->SetTranslate(pos);
	}
}
