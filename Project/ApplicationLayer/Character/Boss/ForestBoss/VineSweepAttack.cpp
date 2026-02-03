#define NOMINMAX
#include "VineSweepAttack.h"
#include <LinearInterpolation.h>
#include <Boss.h>

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>

namespace K4E = ::Ken4lowEngine;

#endif // USE_IMGUI


void VineSweepAttack::Initialize()
{
	// ツタ表示用（例：モデル読み込みなど）
	vineObject_ = std::make_unique<K4E::Object3D>();
	vineObject_->Initialize("cube.gltf");
	vineObject_->SetColor({ 0.4f, 1.0f, 0.4f, 1.0f }); // 緑系（好みで）

	// 残像プール（最初にまとめて生成）
	trailObjects_.clear();
	trailObjects_.reserve(trailMax_);
	for (int i = 0; i < trailMax_; ++i)
	{
		auto obj = std::make_unique<K4E::Object3D>();
		obj->Initialize("cube.gltf");  // ここは「一度だけ」だからOK
		obj->SetScale({ 0,0,0 });        // 非表示スタート
		obj->SetDissolveThreshold(0.0f);
		obj->SetColor({ 0.6f, 1.0f, 0.6f, 1.0f }); // うっすら緑系（好みで）
		trailObjects_.push_back(std::move(obj));
	}
}

void VineSweepAttack::TickCooldown(float deltaTime)
{
	// クールダウンタイマー更新
	if (vine_.cooldownTimer > 0.0f) vine_.cooldownTimer = std::max(0.0f, vine_.cooldownTimer - deltaTime);

	// クールダウン終了で待機へ
	if (vine_.phase == Phase::Cooldown && vine_.cooldownTimer <= 0.0f) vine_.phase = Phase::Idle;
}

bool VineSweepAttack::CanAttack() const
{
	// 攻撃可能か判定
	return vine_.phase == Phase::Idle && vine_.cooldownTimer <= 0.0f;
}

void VineSweepAttack::Attack()
{
	if (!CanAttack()) return;
	requestStart_ = true;
}

void VineSweepAttack::Update(Boss* boss, float deltaTime, float bossYawRad, const K4E::Vector3& playerPosition)
{
	if (!boss) return;

	// configはBossから参照（Attack側のparams_は使わない）
	const auto& a = boss->GetParams().vineSweep;

#ifdef USE_IMGUI
	// HITフラッシュ減衰
	if (debugHitFlash_ > 0.0f)
	{
		debugHitFlash_ = std::max(0.0f, debugHitFlash_ - deltaTime);
	}

	// セクター内判定（可視化用）
	{
		K4E::Vector3 origin = boss->GetLeftArmRootWorldPosition();
		debugPlayerInSector_ = IsPointInSectorXZ(
			playerPosition, origin, vine_.lockedYaw,
			a.radius, a.angleDeg, a.thickness
		);
	}

	// テスト用プレイヤー座標切り替え
	K4E::Vector3 playerPos = debugUseTestPlayerPos_ ? debugTestPlayerPos_ : playerPosition;
#else
	K4E::Vector3 playerPos = playerPosition;
#endif

	// フェーズごとの更新
	switch (vine_.phase)
	{
	case Phase::Idle:	  UpdatePhase_Idle(bossYawRad); break;
	case Phase::Windup:	  UpdatePhase_Windup(boss, deltaTime); break;
	case Phase::Active:	  UpdatePhase_Active(boss, deltaTime, playerPos); break;
	case Phase::Recovery: UpdatePhase_Recovery(boss, deltaTime); break;
	case Phase::Cooldown: TickCooldown(deltaTime); break;
	}

	// フェーズ進行後（phaseTimer更新＆遷移の後）に毎フレーム反映
	if (vine_.visible)
	{
		LeftArmUpdate(boss);
		UpdateVineVisual(boss);

		// ★Active中だけ残像を一定間隔で出す
		if (vine_.phase == Phase::Active && trailEnabled_)
		{
			trailTimer_ += deltaTime;
			while (trailTimer_ >= trailSpawnInterval_)
			{
				trailTimer_ -= trailSpawnInterval_;
				SpawnAfterImage();
			}
		}
	}

	// ツタオブジェクト更新
	vineObject_->Update();

	// 残像更新
	UpdateAfterImages(deltaTime);
}

bool VineSweepAttack::IsActive() const
{
	return vine_.phase != Phase::Idle;
}

void VineSweepAttack::Draw()
{
	// 残像は visibleじゃなくても残ってよい
	if (trailEnabled_)
	{
		for (int i = 0; i < (int)afterImages_.size(); ++i)
		{
			if (i < (int)trailObjects_.size()) trailObjects_[i]->Draw();
		}
	}

	if (!vine_.visible) return;
	vineObject_->Draw();
}

#ifdef USE_IMGUI
void VineSweepAttack::DrawImGui(Boss& boss)
{
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

	// 起動ボタン
	if (ImGui::Button("Test: VineSweep"))
	{
		Attack();
	}
	ImGui::SameLine();
	ImGui::Text("CanAttack: %s", CanAttack() ? "YES" : "NO");

	// Runtime表示
	ImGui::Separator();
	ImGui::Text("Runtime");
	ImGui::Text("Phase: %s", PhaseName(vine_.phase));
	ImGui::Text("visible: %s", vine_.visible ? "true" : "false");
	ImGui::Text("phaseTimer: %.3f", vine_.phaseTimer_);
	ImGui::Text("cooldownTimer: %.3f", vine_.cooldownTimer);
	ImGui::Text("lockedYaw: %.3f", vine_.lockedYaw);
	ImGui::Text("didHit: %s", vine_.didHit ? "true" : "false");

	// HIT表示（Update側で減衰させるのがおすすめ）
	if (debugHitFlash_ > 0.0f)
		ImGui::Text("HIT!! (%.2fs)", debugHitFlash_);
	ImGui::Text("hitCount: %d", debugHitCount_);
	ImGui::Text("playerInSector: %s", debugPlayerInSector_ ? "YES" : "no");

	// テスト用プレイヤー座標
	ImGui::Separator();
	ImGui::Checkbox("Use test player pos", &debugUseTestPlayerPos_);
	ImGui::DragFloat3("TestPlayerPos", &debugTestPlayerPos_.x, 0.05f);

	// 角度可視化（UpdateVineVisualで更新してるdebug値）
	ImGui::Separator();
	ImGui::Text("Angles");
	ImGui::Text("startYaw:  %.3f", debugStartYaw_);
	ImGui::Text("endYaw:    %.3f", debugEndYaw_);
	ImGui::Text("yaw:       %.3f", debugYaw_);
	ImGui::Text("t(active): %.3f", debugT_);

	auto& p = boss.GetBodyParts()[boss.GetPartIndices().leftArm];
	ImGui::Text("LeftArm rot: (%.2f, %.2f, %.2f)", p.transform.rotate_.x, p.transform.rotate_.y, p.transform.rotate_.z);

	// Params編集（Bossが持つ設定を直接いじる）
	if (ImGui::CollapsingHeader("VineSweep K4E::Params", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& a = boss.GetParams().vineSweep;
		ImGui::DragFloat("windup", &a.windup, 0.01f, 0.0f, 5.0f);		  // 溜め時間
		ImGui::DragFloat("windupHold", &a.windupHold, 0.01f, 0.0f, 1.0f); // 溜めホールド時間
		ImGui::DragFloat("active", &a.active, 0.01f, 0.0f, 5.0f);		  // 有効時間
		ImGui::DragFloat("recovery", &a.recovery, 0.01f, 0.0f, 5.0f);	  // 回復時間
		ImGui::DragFloat("cooldown", &a.cooldown, 0.01f, 0.0f, 10.0f);	  // クールダウン時間

		ImGui::DragFloat("radius", &a.radius, 0.1f, 0.0f, 30.0f);		  // 到達距離
		ImGui::DragFloat("angleDeg", &a.angleDeg, 1.0f, 1.0f, 180.0f);	  // 扇形角度
		ImGui::DragFloat("thickness", &a.thickness, 0.05f, 0.0f, 5.0f);	  // Y厚み
	}

	ImGui::Separator();
	ImGui::Text("AfterImage");
	ImGui::Checkbox("Trail Enabled", &trailEnabled_);
	ImGui::DragFloat("SpawnInterval", &trailSpawnInterval_, 0.001f, 0.005f, 0.05f);
	ImGui::DragFloat("Life", &trailLife_, 0.005f, 0.03f, 0.4f);
	ImGui::DragInt("Max", &trailMax_, 1, 1, 32);
	ImGui::Text("ActiveTrailCount: %d", (int)afterImages_.size());
}
#endif // USE_IMGUI


bool VineSweepAttack::IsPointInSectorXZ(const K4E::Vector3& position, const K4E::Vector3& origin, float forwardYawRad, float radius, float angleDeg, float yThickness) const
{
	// 簡易：XZ平面で扇形、Yは厚みで許容
	K4E::Vector3 d = { position.x - origin.x, position.y - origin.y, position.z - origin.z };

	if (std::abs(d.y) > yThickness) return false;

	float dist2 = d.x * d.x + d.z * d.z;
	if (dist2 > radius * radius) return false;

	// forward
	float fx = std::sin(forwardYawRad);
	float fz = std::cos(forwardYawRad);

	float len = std::sqrt(dist2);
	if (len < 1e-5f) return true;

	float nx = d.x / len;
	float nz = d.z / len;

	float dot = nx * fx + nz * fz;
	float cosHalf = std::cos(K4E::DegToRad(angleDeg) * 0.5f);
	return dot >= cosHalf;
}

void VineSweepAttack::UpdateVineVisual(Boss* boss)
{
	if (!vineObject_) return;
	const auto& a = boss->GetParams().vineSweep;
	if (!vine_.visible) return;

	const float half = K4E::DegToRad(a.angleDeg) * 0.5f;
	const float startYaw = vine_.lockedYaw - half;
	const float endYaw = vine_.lockedYaw + half;

	float yaw = vine_.lockedYaw;
	float tActive = 0.0f;

	// Windupの進行度（腕と同じ作り）
	float windT = 0.0f;
	if (vine_.phase == Phase::Windup)
	{
		windT = K4E::clamp01(vine_.phaseTimer_ / std::max(0.001f, a.windup));
		windT = K4E::EaseInOutCubic(windT);
	}

	if (vine_.phase == Phase::Windup)
	{
		// 風向き(lockedYaw)→開始Yaw(startYaw)に“構えながら”寄せると気持ちいい
		yaw = K4E::LerpAngle(vine_.lockedYaw, startYaw, windT);
	}
	else if (vine_.phase == Phase::Active)
	{
		tActive = K4E::clamp01(vine_.phaseTimer_ / std::max(0.001f, a.active));
		tActive = K4E::EaseInOutCubic(tActive);
		yaw = K4E::LerpAngle(startYaw, endYaw, tActive);
	}
	else if (vine_.phase == Phase::Recovery)
	{
		yaw = endYaw;
		tActive = 1.0f;
	}

#ifdef USE_IMGUI
	debugStartYaw_ = startYaw;
	debugEndYaw_ = endYaw;
	debugYaw_ = yaw;
	debugT_ = (vine_.phase == Phase::Windup) ? windT : tActive; // 表示用
#endif

	// 1) ボス中心
	const K4E::Vector3 origin = boss->GetLeftArmRootWorldPosition();

	// 2) yaw方向（XZ）
	const K4E::Vector3 dir{ -std::sin(yaw), 0.0f, std::cos(yaw) };

	// ここがキモ：Windupだけ dist を 0→radius で伸ばす
	float reach = 1.0f;
	if (vine_.phase == Phase::Windup)
	{
		reach = K4E::clamp01(vine_.phaseTimer_ / std::max(0.001f, a.windup));
		reach = K4E::EaseInOutCubic(reach);
	}
	float dist = a.radius * reach;

	const K4E::Vector3 pos{ origin.x + dir.x * dist, origin.y, origin.z + dir.z * dist };

	vineObject_->SetTranslate(pos);
	vineObject_->SetRotate({ 0.0f, yaw, 0.0f });

	lastVinePos_ = pos;
	lastVineYaw_ = yaw;
	lastVineReach_ = reach;
}

void VineSweepAttack::LeftArmUpdate(Boss* boss)
{
	auto& a = boss->GetParams().vineSweep;

	// 0..1 の進行度
	float windT = 0.0f;
	float actT = 0.0f;

	if (vine_.phase == Phase::Windup)
	{
		windT = K4E::clamp01(vine_.phaseTimer_ / std::max(0.001f, a.windup));
		windT = K4E::EaseInOutCubic(windT);
	}
	if (vine_.phase == Phase::Active)
	{
		actT = K4E::clamp01(vine_.phaseTimer_ / std::max(0.001f, a.active));
		actT = K4E::EaseInOutCubic(actT);
	}

	// 目標角度（左腕）
	// X: 上げ下げ、Y: 左右に振る（払う）
	K4E::Vector3 armRot = { 0,0,0 };

	if (vine_.phase == Phase::Windup)
	{
		// 左腕を上げる（例：-60°）
		armRot.x = K4E::Lerp(0.0f, -1.05f, windT);
		// 少し右へ引く（構え）
		armRot.y = K4E::Lerp(0.0f, -0.35f, windT);
	}
	else if (vine_.phase == Phase::Active)
	{
		// 上げたまま、右→左へ払う
		armRot.x = -1.05f;
		// 払い：-60° → +60°（体の前で左右に振る）
		armRot.y = K4E::Lerp(-1.05f, 1.05f, actT);
	}
	else if (vine_.phase == Phase::Recovery)
	{
		// ゆっくり戻す
		float recT = K4E::clamp01(vine_.phaseTimer_ / std::max(0.001f, a.recovery));
		recT = K4E::EaseInOutCubic(recT);
		armRot.x = K4E::Lerp(-1.05f, 0.0f, recT);
		armRot.y = K4E::Lerp(1.05f, 0.0f, recT);
	}
	else
	{
		// Idle/Cooldown は戻しておく
		armRot = { 0,0,0 };
	}

	boss->SetLeftArmLocalRotate(armRot);
}

void VineSweepAttack::SpawnAfterImage()
{
	if (!trailEnabled_) return;

	AfterImage a;
	a.pos = lastVinePos_;
	a.yaw = lastVineYaw_;
	a.reach = lastVineReach_;
	a.age = 0.0f;

	afterImages_.push_back(a);

	// 最大数を超えたら古いものから捨てる
	if ((int)afterImages_.size() > trailMax_)
		afterImages_.erase(afterImages_.begin());
}

void VineSweepAttack::UpdateAfterImages(float deltaTime)
{
	if (!trailEnabled_) return;

	// 年齢更新＆寿命超え削除
	for (auto& a : afterImages_) a.age += deltaTime;
	afterImages_.erase(
		std::remove_if(afterImages_.begin(), afterImages_.end(),
			[&](const AfterImage& a) { return a.age >= trailLife_; }),
		afterImages_.end()
	);

	// Object3Dへ反映（afterImages_ と trailObjects_ を対応させる）
	const int n = (int)afterImages_.size();
	for (int i = 0; i < (int)trailObjects_.size(); ++i)
	{
		auto* obj = trailObjects_[i].get();
		if (!obj) continue;

		if (i < n)
		{
			const auto& img = afterImages_[i];
			const float t = std::clamp(img.age / std::max(0.001f, trailLife_), 0.0f, 1.0f);

			// ブレンドが効かない可能性があるので「ディゾルブ」で消すのを主にする
			obj->SetDissolveThreshold(1.0f - t);

			// スケールも少し縮める（薄れた感）
			const float s = K4E::Lerp(1.0f, 0.7f, t) * std::max(0.2f, img.reach);
			obj->SetScale({ s, s, s });

			obj->SetTranslate(img.pos);
			obj->SetRotate({ 0.0f, img.yaw, 0.0f });

			obj->Update();
		}
		else
		{
			// 使ってない残像は消す
			obj->SetScale({ 0,0,0 });
			obj->SetDissolveThreshold(0.0f);
			obj->Update();
		}
	}
}

void VineSweepAttack::UpdatePhase_Idle(float bossYawRad)
{
	if (requestStart_)
	{
		requestStart_ = false;

		if (CanAttack()) {
			vine_.phase = Phase::Windup;
			vine_.phaseTimer_ = 0.0f;
			vine_.didHit = false;
			vine_.visible = true;
			vine_.lockedYaw = bossYawRad; // このフレームの向きを固定
		}
	}
}

void VineSweepAttack::UpdatePhase_Windup(Boss* boss, float deltaTime)
{
	// configはBossから参照（Attack側のparams_は使わない）
	const auto& a = boss->GetParams().vineSweep;

	vine_.phaseTimer_ += deltaTime;

	// 溜め + ホールドが終わったら Active へ
	if (vine_.phaseTimer_ >= (a.windup + a.windupHold))
	{
		vine_.phase = Phase::Active;
		vine_.phaseTimer_ = 0.0f;
	}
}

void VineSweepAttack::UpdatePhase_Active(Boss* boss, float deltaTime, const K4E::Vector3& playerPosition)
{
	// configはBossから参照（Attack側のparams_は使わない）
	const auto& a = boss->GetParams().vineSweep;

	vine_.phaseTimer_ += deltaTime;

	// まだ当てていなければ Hit 判定を行う
	if (!vine_.didHit) {
		K4E::Vector3 origin = boss->GetLeftArmRootWorldPosition();
		if (IsPointInSectorXZ(playerPosition, origin, vine_.lockedYaw, a.radius, a.angleDeg, a.thickness)) {
			vine_.didHit = true;
#ifdef USE_IMGUI
			debugHitCount_++;
			debugHitFlash_ = 0.8f;
#endif
			// TODO: プレイヤーへダメージ適用
		}
	}

	// Active 終了で Recovery へ
	if (vine_.phaseTimer_ >= a.active)
	{
		vine_.phase = Phase::Recovery;
		vine_.phaseTimer_ = 0.0f;
	}
}

void VineSweepAttack::UpdatePhase_Recovery(Boss* boss, float deltaTime)
{
	// configはBossから参照（Attack側のparams_は使わない）
	const auto& a = boss->GetParams().vineSweep;

	vine_.phaseTimer_ += deltaTime;

	// 戻しが終わったら Cooldown へ
	if (vine_.phaseTimer_ >= a.recovery)
	{
		vine_.phase = Phase::Cooldown;
		vine_.cooldownTimer = a.cooldown;
		vine_.phaseTimer_ = 0.0f;
		vine_.visible = false;
	}
}