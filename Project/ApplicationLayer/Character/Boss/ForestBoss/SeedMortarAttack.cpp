#define NOMINMAX
#include "SeedMortarAttack.h"
#include <Boss.h>
#include <LinearInterpolation.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

#include <algorithm>
#include <random>
#include <numbers>

namespace K4E = ::Ken4lowEngine;

namespace
{
	float Random01()
	{
		// スレッドごとに独立した乱数エンジン
		static thread_local std::mt19937 engine{
			std::random_device{}()
		};

		// [0,1) の一様分布
		static std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

		return dist01(engine);
	}
} // namespace

void SeedMortarAttack::Initialize()
{
	seeds_.clear();
}

void SeedMortarAttack::TickCooldown(float deltaTime)
{
	if (runtime_.cooldownTime > 0.0f)
	{
		runtime_.cooldownTime = std::max(0.0f, runtime_.cooldownTime - deltaTime);

		// クールダウン中にタイマーが0になったら Idle に戻る
		if (runtime_.cooldownTime <= 0.0f && runtime_.phase == Phase::Cooldown)
		{
			runtime_.phase = Phase::Idle;
		}
	}
}

bool SeedMortarAttack::CanAttack() const
{
	// 攻撃可能か判定
	return runtime_.phase == Phase::Idle && runtime_.cooldownTime <= 0.0f;
}

void SeedMortarAttack::Attack()
{
	if (!CanAttack()) return;
	requestStart_ = true;
}

void SeedMortarAttack::Update(Boss* boss, float deltaTime, float bossYawRad, const K4E::Vector3& playerPosition)
{
	if (!boss) { return; }

	K4E::Vector3 playerPos = playerPosition;
#ifdef USE_IMGUI
	if (debugUseTestPlayerPos_)
	{
		playerPos = debugTestPlayerPos_;
	}
#endif

	// Idle / Cooldown 以外はフェーズタイマーを進める
	if (runtime_.phase != Phase::Idle && runtime_.phase != Phase::Cooldown)
	{
		runtime_.phaseTimer += deltaTime;
	}

	// フェーズごとの更新
	switch (runtime_.phase)
	{
	case Phase::Idle:      UpdatePhase_Idle(bossYawRad);                   break;
	case Phase::Windup:    UpdatePhase_Windup(boss, deltaTime);            break;
	case Phase::Active:    UpdatePhase_Active(boss, deltaTime, playerPos); break;
	case Phase::Recovery:  UpdatePhase_Recovery(boss, deltaTime);          break;
	case Phase::Cooldown:  TickCooldown(deltaTime);                        break;
	default:               break;
	}

	// フェーズに応じてボスの腕ポーズを更新
	UpdateBossPose(boss);

	// 種オブジェクトの位置更新
	for (auto& seed : seeds_)
	{
		if (!seed.active) { continue; }
		if (!seed.object) { continue; }

		seed.object->SetTranslate(seed.position);
		seed.object->Update();
	}
}

bool SeedMortarAttack::IsActive() const
{
	return runtime_.phase != Phase::Idle && runtime_.phase != Phase::Cooldown;
}

void SeedMortarAttack::Draw()
{
	if (!runtime_.visible) { return; }

	for (auto& seed : seeds_)
	{
		if (!seed.active) { continue; }
		if (!seed.object) { continue; }

		seed.object->Draw();
	}
}

#ifdef USE_IMGUI
void SeedMortarAttack::DrawImGui(Boss& boss)
{

	// ★ 編集したいので const を外す
	auto& params = boss.GetParams().seedMortar;

	// 便利：フェーズ名
	auto PhaseName = [](Phase p)->const char*
		{
			switch (p)
			{
			case Phase::Idle:     return "Idle";
			case Phase::Windup:   return "Windup";
			case Phase::Active:   return "Active";
			case Phase::Recovery: return "Recovery";
			case Phase::Cooldown: return "Cooldown";
			default:              return "?";
			}
		};

	if (ImGui::Button("Test: SeedMortar"))
	{
		Attack();
	}
	ImGui::SameLine();
	ImGui::Text("CanAttack: %s", CanAttack() ? "YES" : "NO");

	ImGui::Separator();
	ImGui::Text("Runtime");
	ImGui::Text("Phase       : %s", PhaseName(runtime_.phase));
	ImGui::Text("phaseTimer  : %.3f", runtime_.phaseTimer);
	ImGui::Text("cooldown    : %.3f", runtime_.cooldownTime);
	ImGui::Text("lockedYaw   : %.3f", runtime_.lockedYaw);
	ImGui::Text("visible     : %s", runtime_.visible ? "true" : "false");
	ImGui::Text("seeds.size  : %d", static_cast<int>(seeds_.size()));

	int activeCount = 0;
	for (auto& s : seeds_)
	{
		if (s.active) { ++activeCount; }
	}
	ImGui::Text("activeSeeds : %d", activeCount);

	ImGui::Separator();
	ImGui::Checkbox("Use Test PlayerPos", &debugUseTestPlayerPos_);
	ImGui::DragFloat3("Test PlayerPos", &debugTestPlayerPos_.x, 0.1f);

	// Config 編集ブロック
	if (ImGui::CollapsingHeader("SeedMortar K4E::Params", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextUnformatted("Timing");
		ImGui::DragFloat("windup", &params.windup, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("riseTime", &params.riseTime, 0.01f, 0.05f, 5.0f);
		ImGui::DragFloat("recovery", &params.recovery, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("cooldown", &params.cooldown, 0.01f, 0.0f, 10.0f);

		ImGui::Separator();
		ImGui::TextUnformatted("Pattern");
		ImGui::DragInt("count", &params.count, 1, 1, 16);
		ImGui::DragFloat("spreadRadius", &params.spreadRadius, 0.05f, 0.0f, 20.0f);
		ImGui::DragFloat("impactRadius", &params.impactRadius, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("minSeedSpacing", &params.minSeedSpacing, 0.01f, 0.0f, 10.0f);

		ImGui::Separator();
		ImGui::TextUnformatted("Emerge");
		ImGui::DragFloat("emergeDepth", &params.emergeDepth, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("emergeHeight", &params.emergeHeight, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("avoidBossRadius", &params.avoidBossRadius, 0.01f, 0.0f, 5.0f);

		ImGui::Separator();
		ImGui::TextUnformatted("Hit");
		ImGui::DragFloat("damage", &params.hit.damage, 0.5f, 0.0f, 999.0f);
		ImGui::DragFloat("knockbackH", &params.hit.knockbackH, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("knockbackUp", &params.hit.knockbackUp, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("hitStop", &params.hit.hitStop, 0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::TextUnformatted("Telegraph");
		ImGui::DragFloat("telegraph.time", &params.telegraph.time, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("telegraph.decalScale", &params.telegraph.decalScale, 0.01f, 0.1f, 5.0f);

		ImGui::Separator();
		ImGui::DragFloat("exposeCoreAfter", &params.exposeCoreAfter, 0.01f, 0.0f, 5.0f);
	}

	// 腕ポーズの手動編集ブロック
	if (ImGui::CollapsingHeader("SeedMortar Pose"))
	{
		ImGui::DragFloat("raiseX", &poseParams_.raiseX, 0.01f, -3.14f, 3.14f);
		ImGui::DragFloat("openYawMin", &poseParams_.openYawMin, 0.01f, -3.14f, 3.14f);
		ImGui::DragFloat("openYawMax", &poseParams_.openYawMax, 0.01f, -3.14f, 3.14f);
		ImGui::DragFloat("clapX", &poseParams_.clapX, 0.01f, -3.14f, 3.14f);
		ImGui::DragFloat("holdUntil", &poseParams_.holdUntil, 0.01f, 0.0f, 1.0f);

		if (ImGui::Button("Reset Pose K4E::Params"))
		{
			poseParams_ = PoseParams{}; // デフォルト値に戻す
		}
	}
}
#endif // USE_IMGUI

void SeedMortarAttack::UpdatePhase_Idle(float bossYawRad)
{
	if (!requestStart_) { return; }

	requestStart_ = false;

	if (!CanAttack()) { return; }

	// 攻撃開始
	runtime_.phase = Phase::Windup;
	runtime_.phaseTimer = 0.0f;
	runtime_.didHit = false;
	runtime_.visible = false;
	runtime_.lockedYaw = bossYawRad;

	spawned_ = false;
	seeds_.clear();
}

void SeedMortarAttack::UpdatePhase_Windup(Boss* boss, float deltaTime)
{
	(void)deltaTime;

	const auto& params = boss->GetParams().seedMortar;

	// 一定時間溜めたら Active へ
	if (runtime_.phaseTimer >= params.windup)
	{
		runtime_.phase = Phase::Active;
		runtime_.phaseTimer = 0.0f;
		spawned_ = false; // Active に入った瞬間にスポーンする
	}
}

void SeedMortarAttack::UpdatePhase_Active(Boss* boss, float deltaTime, const K4E::Vector3& playerPosition)
{
	const auto& params = boss->GetParams().seedMortar;

	// Active に入った直後に種をスポーン
	if (!spawned_)
	{
		// プレイヤー位置（XZ）とボス位置（XZ）を用意
		K4E::Vector3 playerXZ = playerPosition;
		playerXZ.y = 0.0f;

		K4E::Vector3 bossCenter = boss->GetCenterPosition();
		K4E::Vector3 bossXZ = bossCenter;
		bossXZ.y = 0.0f;

		SpawnSeeds(playerXZ, bossXZ, params);
		runtime_.visible = true;
		spawned_ = true;
	}

	bool anyActive = false;

	for (auto& seed : seeds_)
	{
		if (!seed.active) { continue; }

		anyActive = true;
		seed.timer += deltaTime;

		// ★ Config の riseTime を使用
		float t = std::clamp(seed.timer / params.riseTime, 0.0f, 1.0f);

		// XZ はずっと groundPos
		seed.position.x = seed.groundPos.x;
		seed.position.z = seed.groundPos.z;

		// ★ Y を「地面の下 → 地面上 → 上」へ
		//    emergeDepth / emergeHeight を Config から使用
		float baseY = seed.groundPos.y;
		seed.position.y =
			baseY - params.emergeDepth
			+ (params.emergeDepth + params.emergeHeight) * t;

		if (t >= 1.0f && !seed.exploded)
		{
			seed.exploded = true;
			seed.active = false;

			// TODO: 爆発ダメージ＆エフェクト
		}
	}

	if (!anyActive)
	{
		runtime_.phase = Phase::Recovery;
		runtime_.phaseTimer = 0.0f;
		runtime_.visible = false;
	}
}

void SeedMortarAttack::UpdatePhase_Recovery(Boss* boss, float deltaTime)
{
	(void)deltaTime;

	const auto& params = boss->GetParams().seedMortar;

	// ★ Config の recovery を使用
	if (runtime_.phaseTimer >= params.recovery)
	{
		runtime_.phase = Phase::Cooldown;
		runtime_.phaseTimer = 0.0f;

		// ★ Config の cooldown を使用
		runtime_.cooldownTime = params.cooldown;

		// 次回用にリセット
		seeds_.clear();
		spawned_ = false;
	}
}

void SeedMortarAttack::UpdateBossPose(Boss* boss)
{
	if (!boss) { return; }
	if (runtime_.phase == Phase::Idle || runtime_.phase == Phase::Cooldown)
	{
		return;
	}

	const auto& params = boss->GetParams().seedMortar;
	const PoseParams& pp = poseParams_;

	K4E::Vector3 leftRot{};
	K4E::Vector3 rightRot{};

	// --- Windup：ニュートラル → 腕を上げる (Y=0 のまま) ---
	if (runtime_.phase == Phase::Windup)
	{
		float t = K4E::clamp01(runtime_.phaseTimer / std::max(0.001f, params.windup));
		t = K4E::EaseInOutCubic(t);

		float x = K4E::Lerp(0.0f, pp.raiseX, t);

		leftRot = { x, 0.0f, 0.0f };
		rightRot = { x, 0.0f, 0.0f };
	}
	// --- Active：腕を上げたまま → 外に広げて → 内側に閉じる ---
	else if (runtime_.phase == Phase::Active)
	{
		// 0〜1 に正規化した進行度（riseTime ベース）
		float t = K4E::clamp01(runtime_.phaseTimer / std::max(0.001f, params.riseTime));

		// 「どこまで溜めるか」(0〜1)
		float hold = std::clamp(poseParams_.holdUntil, 0.0f, 1.0f);

		// 残り区間 (hold〜1.0) を「開く」と「閉じる」で 2 分割
		float openEnd = hold + (1.0f - hold) * 0.5f; // 例: hold=0.8 → openEnd=0.9

		const float yawMin = poseParams_.openYawMin; // 例: -0.6
		const float yawMax = poseParams_.openYawMax; // 例: +0.4
		float x = poseParams_.raiseX;
		float yaw = 0.0f;

		if (t < hold)
		{
			// ① 溜め：腕を上げたまま、まだ広げない（Y=0）
			yaw = 0.0f;
		}
		else if (t < openEnd)
		{
			// ② 開く：0 → yawMin（外側）へ
			float u = (t - hold) / std::max(0.001f, openEnd - hold); // 0〜1
			u = K4E::clamp01(u);
			u = K4E::EaseInOutCubic(u);

			yaw = K4E::Lerp(0.0f, yawMin, u);
		}
		else
		{
			// ③ 閉じる：yawMin（外）→ yawMax（内寄り）へ
			float u = (t - openEnd) / std::max(0.001f, 1.0f - openEnd); // 0〜1
			u = K4E::clamp01(u);
			u = K4E::EaseInOutCubic(u);

			yaw = K4E::Lerp(yawMin, yawMax, u);
		}

		// 左右で ± を反転させることで「外 → 中央」っぽい動きにする
		leftRot = { x,  yaw, 0.0f };
		rightRot = { x, -yaw, 0.0f };
	}
	// --- Recovery：合掌状態からニュートラルへ戻す ---
	else if (runtime_.phase == Phase::Recovery)
	{
		float t = K4E::clamp01(runtime_.phaseTimer / std::max(0.001f, params.recovery));
		t = K4E::EaseInOutCubic(t);

		// Active 終了時点では yaw ≒ yawMax, x ≒ raiseX とみなす
		float x = K4E::Lerp(pp.clapX, 0.0f, t);
		float yawL = K4E::Lerp(pp.openYawMax, 0.0f, t);
		float yawR = -yawL;

		leftRot = { x, yawL, 0.0f };
		rightRot = { x, yawR, 0.0f };
	}

	boss->SetLeftArmLocalRotate(leftRot);
	boss->SetRightArmLocalRotate(rightRot);
}


void SeedMortarAttack::SpawnSeeds(const K4E::Vector3& centerXZ, const K4E::Vector3& bossCenterXZ, const ForestBossParams::SeedMortar& params)
{
	seeds_.clear();
	seeds_.resize(static_cast<size_t>(std::max(1, params.count)));

	const float avoidR = std::max(0.0f, params.avoidBossRadius);
	const float avoidR2 = avoidR * avoidR;

	const float spacing = std::max(0.0f, params.minSeedSpacing);
	const float spacing2 = spacing * spacing;

	// すでに確定した種の配置リスト（XZ 平面）
	std::vector<K4E::Vector3> placedPositions;
	placedPositions.reserve(seeds_.size());

	for (auto& seed : seeds_)
	{
		K4E::Vector3 groundPos{};
		const int kMaxRetry = 32; // 種同士の距離チェックも増えるので少し多めに
		int retry = 0;

		while (true)
		{
			// 位置候補をランダムに生成
			const float angle = Random01() * 2.0f * std::numbers::pi_v<float>;
			const float dist = Random01() * params.spreadRadius;

			const float offsetX = std::cos(angle) * dist;
			const float offsetZ = std::sin(angle) * dist;

			groundPos = centerXZ;
			groundPos.x += offsetX;
			groundPos.z += offsetZ;

			// --- 1) ボスから十分離れているか（XZ） ---
			K4E::Vector3 toBoss{
				groundPos.x - bossCenterXZ.x,
				0.0f,
				groundPos.z - bossCenterXZ.z
			};
			float distBoss2 = toBoss.x * toBoss.x + toBoss.z * toBoss.z;
			if (distBoss2 < avoidR2)
			{
				if (retry++ >= kMaxRetry) { break; } // 妥協
				continue; // 近すぎるので再抽選
			}

			// --- 2) 他の種から十分離れているか（XZ） ---
			bool tooClose = false;
			for (const auto& p : placedPositions)
			{
				K4E::Vector3 d{
					groundPos.x - p.x,
					0.0f,
					groundPos.z - p.z
				};
				float dist2 = d.x * d.x + d.z * d.z;
				if (dist2 < spacing2)
				{
					tooClose = true;
					break;
				}
			}

			if (tooClose)
			{
				if (retry++ >= kMaxRetry) { break; } // 妥協
				continue; // 他の種に近すぎるので再抽選
			}

			// ボスからも他の種からも十分離れているので、この位置を採用
			break;
		}

		// この時点の groundPos を確定位置として使う
		seed.active = true;
		seed.exploded = false;
		seed.timer = 0.0f;

		seed.groundPos = groundPos;

		// 最初は地面の少し下からスタート
		seed.position = seed.groundPos;
		seed.position.y -= params.emergeDepth;

		// K4E::Object3D がまだなければ作る
		if (!seed.object)
		{
			seed.object = std::make_unique<K4E::Object3D>();
			seed.object->Initialize("cube.gltf");
		}

		// 配置リストに追加（XZ分だけで十分）
		placedPositions.push_back(seed.groundPos);
	}
}
