#define NOMINMAX
#include "BossIntroController.h"

#include "Camera.h"
#include "CameraManager.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "GpuParticleManager.h"
#include "ParameterManager.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;
}

void BossIntroController::Initialize(const K4E::Vector3& defaultBossPosition)
{
	settings_.bossAppearPosition = defaultBossPosition;
	// ボス登場演出で使う土煙GPUパーティクルを、カットシーン管理側でまとめて登録する。
	bossEnemyVfx_.Initialize(K4E::GpuParticleManager::GetInstance(), "BossIntro");
	RegisterParameters();
	ApplyParameters();
	Reset();
}

void BossIntroController::Finalize()
{
	UnregisterParameters();
	// リトライやシーン破棄時に、古いEmitter参照を保持し続けないようにする。
	bossEnemyVfx_.Reset();
	ResetBossIntroDustState();
}

void BossIntroController::RequestStart(const K4E::Vector3& bossPosition)
{
	if (hasPlayedBossIntro_ || IsRunning())
	{
		return;
	}

	settings_.bossAppearPosition = bossPosition;
	ApplyParameters();
	stateTimer_ = 0.0f;
	ResetBossIntroDustState();
	ChangeState(State::WaitingAfterCrystalsBroken);
	K4E::Log("[BossIntro] Waiting after all crystals broken.\n");
}

void BossIntroController::Reset()
{
	state_ = State::None;
	stateTimer_ = 0.0f;
	hasPlayedBossIntro_ = false;
	bossSpawnRequested_ = false;
	bossColliderEnableRequested_ = false;
	debugStartRequested_ = false;
	debugResetRequested_ = false;
	debugForceBossToAppearRequested_ = false;
	debugClearBossParentRequested_ = false;
	debugUseGameplayViewProjectionRequested_ = false;
	ResetBossIntroDustState();
}

void BossIntroController::Update(float deltaTime, GuardianBoss* boss, K4E::Camera* camera)
{
	ApplyParameters();
	SetDebugSnapshot(boss, camera);

	switch (state_)
	{
	case State::WaitingAfterCrystalsBroken:
		stateTimer_ += deltaTime;
		if (stateTimer_ >= std::max(0.0f, settings_.bossAppearDelay))
		{
			// 全クリスタル破壊後、緊張を作るための遅延が終わってから登場カットシーンへ入る。
			ChangeState(State::StartCutscene);
		}
		break;

	case State::StartCutscene:
		BeginCutscene(camera);
		break;

	case State::CameraMoveToBoss:
		UpdateCameraMove(deltaTime, camera);
		break;

	case State::BossRising:
		UpdateBossRising(deltaTime, boss, camera);
		break;

	case State::FinishCutscene:
		UpdateCameraReturn(deltaTime, boss, camera);
		break;

	case State::None:
	case State::Completed:
	default:
		break;
	}
}

void BossIntroController::SetDebugSnapshot(const GuardianBoss* boss, const K4E::Camera* camera)
{
	debugHasBoss_ = boss != nullptr;
	debugHasCamera_ = camera != nullptr;
	debugBossPosition_ = boss ? boss->GetPosition() : K4E::Vector3{};
	debugBossLocalPosition_ = boss ? boss->GetRootLocalPosition() : K4E::Vector3{};
	debugBossWorldPosition_ = boss ? boss->GetRootWorldPosition() : K4E::Vector3{};
	debugBossHasParent_ = boss ? boss->HasRootParent() : false;
	debugCameraPosition_ = camera ? camera->GetTranslate() : K4E::Vector3{};
	debugBossCameraDistance_ = (boss && camera) ? K4E::Vector3::Length(debugBossWorldPosition_ - debugCameraPosition_) : 0.0f;
	debugViewProjectionKind_ = (camera && K4E::CameraManager::GetInstance()->GetMainCamera() == camera)
		? "Gameplay/MainCamera"
		: "CameraManager Other/Debug";
}

void BossIntroController::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("BossIntro", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	ImGui::Text("State: %s", ToStateLabel(state_));
	ImGui::Text("Played: %s", hasPlayedBossIntro_ ? "true" : "false");
	ImGui::Text("Active: %s", IsRunning() ? "true" : "false");
	ImGui::Text("Gameplay Pause: %s", IsGameplayPaused() ? "true" : "false");
	ImGui::Text("Timer: %.2f", stateTimer_);
	ImGui::Text("Boss Position: %.2f, %.2f, %.2f%s",
		debugBossPosition_.x, debugBossPosition_.y, debugBossPosition_.z, debugHasBoss_ ? "" : " (none)");
	ImGui::Text("Boss Local Position: %.2f, %.2f, %.2f",
		debugBossLocalPosition_.x, debugBossLocalPosition_.y, debugBossLocalPosition_.z);
	ImGui::Text("Boss World Position: %.2f, %.2f, %.2f",
		debugBossWorldPosition_.x, debugBossWorldPosition_.y, debugBossWorldPosition_.z);
	ImGui::Text("Boss Parent nullptr: %s", debugBossHasParent_ ? "false" : "true");
	ImGui::Text("Boss Final Position: %.2f, %.2f, %.2f",
		settings_.bossAppearPosition.x, settings_.bossAppearPosition.y, settings_.bossAppearPosition.z);
	ImGui::Text("Camera Position: %.2f, %.2f, %.2f%s",
		debugCameraPosition_.x, debugCameraPosition_.y, debugCameraPosition_.z, debugHasCamera_ ? "" : " (none)");
	ImGui::Text("ViewProjection: %s", debugViewProjectionKind_);
	ImGui::Text("Boss-Camera Distance: %.2f", debugBossCameraDistance_);
	ImGui::Text("Dust Enabled: %s", settings_.enableBossIntroDust ? "true" : "false");
	ImGui::Text("Dust Timer: %.2f", bossIntroDustEmitTimer_);

	if (ImGui::Button("Start Boss Intro"))
	{
		debugStartRequested_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Boss Intro"))
	{
		debugResetRequested_ = true;
	}
	if (ImGui::Button("Force Boss To Appear Position"))
	{
		debugForceBossToAppearRequested_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear Boss Parent"))
	{
		debugClearBossParentRequested_ = true;
	}
	if (ImGui::Button("Use Gameplay ViewProjection"))
	{
		debugUseGameplayViewProjectionRequested_ = true;
	}

	ImGui::Text("Parameter group: %s", kParameterGroupName);
#endif
}

bool BossIntroController::IsGameplayPaused() const
{
	if (!settings_.bossIntroPauseGame)
	{
		return false;
	}

	return state_ == State::StartCutscene ||
		state_ == State::CameraMoveToBoss ||
		state_ == State::BossRising ||
		state_ == State::FinishCutscene;
}

bool BossIntroController::ConsumeBossSpawnRequest()
{
	const bool requested = bossSpawnRequested_;
	bossSpawnRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeBossColliderEnableRequest()
{
	const bool requested = bossColliderEnableRequested_;
	bossColliderEnableRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugStartRequest()
{
	const bool requested = debugStartRequested_;
	debugStartRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugResetRequest()
{
	const bool requested = debugResetRequested_;
	debugResetRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugForceBossToAppearRequest()
{
	const bool requested = debugForceBossToAppearRequested_;
	debugForceBossToAppearRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugClearBossParentRequest()
{
	const bool requested = debugClearBossParentRequested_;
	debugClearBossParentRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugUseGameplayViewProjectionRequest()
{
	const bool requested = debugUseGameplayViewProjectionRequested_;
	debugUseGameplayViewProjectionRequested_ = false;
	return requested;
}

K4E::Vector3 BossIntroController::GetBossStartPosition() const
{
	K4E::Vector3 start = settings_.bossAppearPosition;
	start.y += settings_.enableBossRiseEffect ? settings_.bossStartOffsetY : 0.0f;
	return start;
}

void BossIntroController::RegisterParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	parameters->CreateGroup(kParameterGroupName);

	// ボス登場演出はParameterManagerに乗せ、Json/ImGuiから遅延や位置を調整できるようにする。
	parameters->AddItem(kParameterGroupName, "bossAppearDelay", settings_.bossAppearDelay, 0.0f, 20.0f);
	parameters->AddItem(kParameterGroupName, "bossAppearPosition", settings_.bossAppearPosition, K4E::Vector3{ -200.0f, -50.0f, -200.0f }, K4E::Vector3{ 200.0f, 100.0f, 200.0f });
	parameters->AddItem(kParameterGroupName, "bossStartOffsetY", settings_.bossStartOffsetY, -100.0f, 0.0f);
	parameters->AddItem(kParameterGroupName, "bossRiseTime", settings_.bossRiseTime, 0.1f, 20.0f);
	parameters->AddItem(kParameterGroupName, "cameraMoveTime", settings_.cameraMoveTime, 0.0f, 10.0f);
	parameters->AddItem(kParameterGroupName, "cameraReturnTime", settings_.cameraReturnTime, 0.0f, 10.0f);
	parameters->AddItem(kParameterGroupName, "bossIntroPauseGame", settings_.bossIntroPauseGame);
	parameters->AddItem(kParameterGroupName, "enableBossIntroCamera", settings_.enableBossIntroCamera);
	parameters->AddItem(kParameterGroupName, "enableBossRiseEffect", settings_.enableBossRiseEffect);
	parameters->AddItem(kParameterGroupName, "enableBossIntroDust", settings_.enableBossIntroDust);
	parameters->AddItem(kParameterGroupName, "dustGroundOffsetY", settings_.dustGroundOffsetY, 0.0f, 10.0f);
	parameters->AddItem(kParameterGroupName, "dustEmitInterval", settings_.dustEmitInterval, 0.01f, 1.0f);
	parameters->AddItem(kParameterGroupName, "dustStartBurstCount", static_cast<int>(settings_.dustStartBurstCount), 0, 512);
	parameters->AddItem(kParameterGroupName, "dustLoopEmitCount", static_cast<int>(settings_.dustLoopEmitCount), 0, 128);
	parameters->AddItem(kParameterGroupName, "dustEndBurstCount", static_cast<int>(settings_.dustEndBurstCount), 0, 512);

	parameters->SetDisplayName(kParameterGroupName, "bossAppearDelay", "ボス登場までの遅延");
	parameters->SetDisplayName(kParameterGroupName, "bossAppearPosition", "ボス最終出現座標");
	parameters->SetDisplayName(kParameterGroupName, "bossStartOffsetY", "ボス開始Yオフセット");
	parameters->SetDisplayName(kParameterGroupName, "bossRiseTime", "ボス上昇時間");
	parameters->SetDisplayName(kParameterGroupName, "cameraMoveTime", "カメラ移動時間");
	parameters->SetDisplayName(kParameterGroupName, "cameraReturnTime", "カメラ復帰時間");
	parameters->SetDisplayName(kParameterGroupName, "bossIntroPauseGame", "登場中ゲーム進行停止");
	parameters->SetDisplayName(kParameterGroupName, "enableBossIntroCamera", "登場カメラ有効");
	parameters->SetDisplayName(kParameterGroupName, "enableBossRiseEffect", "下から登場有効");
	parameters->SetDisplayName(kParameterGroupName, "enableBossIntroDust", "登場土煙有効");
	parameters->SetDisplayName(kParameterGroupName, "dustGroundOffsetY", "土煙の足元Y補正");
	parameters->SetDisplayName(kParameterGroupName, "dustEmitInterval", "土煙の連続発生間隔");
	parameters->SetDisplayName(kParameterGroupName, "dustStartBurstCount", "登場開始土煙数");
	parameters->SetDisplayName(kParameterGroupName, "dustLoopEmitCount", "登場中土煙数");
	parameters->SetDisplayName(kParameterGroupName, "dustEndBurstCount", "登場完了土煙数");
	parameters->LoadFile(kParameterGroupName);
	parameters->RegisterParameterApplier(kParameterGroupName, this, [this]() { ApplyParameters(); });
}

void BossIntroController::UnregisterParameters()
{
	K4E::ParameterManager::GetInstance()->UnregisterParameterApplier(kParameterGroupName, this);
}

void BossIntroController::ApplyParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	settings_.bossAppearDelay = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "bossAppearDelay"));
	settings_.bossAppearPosition = parameters->GetValue<K4E::Vector3>(kParameterGroupName, "bossAppearPosition");
	settings_.bossStartOffsetY = std::min(0.0f, parameters->GetValue<float>(kParameterGroupName, "bossStartOffsetY"));
	settings_.bossRiseTime = std::max(0.01f, parameters->GetValue<float>(kParameterGroupName, "bossRiseTime"));
	settings_.cameraMoveTime = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "cameraMoveTime"));
	settings_.cameraReturnTime = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "cameraReturnTime"));
	settings_.bossIntroPauseGame = parameters->GetValue<bool>(kParameterGroupName, "bossIntroPauseGame");
	settings_.enableBossIntroCamera = parameters->GetValue<bool>(kParameterGroupName, "enableBossIntroCamera");
	settings_.enableBossRiseEffect = parameters->GetValue<bool>(kParameterGroupName, "enableBossRiseEffect");
	settings_.enableBossIntroDust = parameters->GetValue<bool>(kParameterGroupName, "enableBossIntroDust");
	settings_.dustGroundOffsetY = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "dustGroundOffsetY"));
	settings_.dustEmitInterval = std::max(0.01f, parameters->GetValue<float>(kParameterGroupName, "dustEmitInterval"));
	settings_.dustStartBurstCount = static_cast<uint32_t>(std::max(0, parameters->GetValue<int>(kParameterGroupName, "dustStartBurstCount")));
	settings_.dustLoopEmitCount = static_cast<uint32_t>(std::max(0, parameters->GetValue<int>(kParameterGroupName, "dustLoopEmitCount")));
	settings_.dustEndBurstCount = static_cast<uint32_t>(std::max(0, parameters->GetValue<int>(kParameterGroupName, "dustEndBurstCount")));
}

void BossIntroController::ChangeState(State state)
{
	state_ = state;
	stateTimer_ = 0.0f;
}

void BossIntroController::BeginCutscene(K4E::Camera* camera)
{
	if (camera)
	{
		savedCameraPosition_ = camera->GetTranslate();
		savedCameraRotation_ = camera->GetRotate();
	}

	introCameraTarget_ = settings_.bossAppearPosition;
	introCameraTarget_.y += 2.5f;
	introCameraPosition_ = settings_.bossAppearPosition + K4E::Vector3{ 0.0f, 7.0f, -18.0f };

	// ボス登場カットシーンの開始に合わせ、足元の土煙演出も初期化する。
	ResetBossIntroDustState();
	bossSpawnRequested_ = true;
	ChangeState(settings_.enableBossIntroCamera ? State::CameraMoveToBoss : State::BossRising);
	K4E::Log("[BossIntro] Cutscene started.\n");
}

void BossIntroController::UpdateCameraMove(float deltaTime, K4E::Camera* camera)
{
	stateTimer_ += deltaTime;
	const float t = settings_.cameraMoveTime <= 0.0f ? 1.0f : Clamp01(stateTimer_ / settings_.cameraMoveTime);

	if (camera)
	{
		// ボス出現地点が画面中央へ入るよう、保存した通常カメラから演出カメラへ補間する。
		const K4E::Vector3 cameraPosition = Lerp(savedCameraPosition_, introCameraPosition_, t);
		ApplyCameraLookAtBoss(camera, cameraPosition, introCameraTarget_);
	}

	if (t >= 1.0f)
	{
		ChangeState(State::BossRising);
	}
}

void BossIntroController::UpdateBossRising(float deltaTime, GuardianBoss* boss, K4E::Camera* camera)
{
	stateTimer_ += deltaTime;
	const float t = settings_.bossRiseTime <= 0.0f ? 1.0f : Clamp01(stateTimer_ / settings_.bossRiseTime);

	// ボスが地面から上がる間、足元へ土煙を出して登場演出の密度を上げる。
	UpdateBossIntroDust(deltaTime, t);

	if (boss)
	{
		// ボスを最終出現位置の下から上へ補間し、登場完了までAI更新はWorld側で止めておく。
		const K4E::Vector3 position = Lerp(GetBossStartPosition(), settings_.bossAppearPosition, t);
		boss->SetPosition(position);
		boss->SetYaw(kPi);
		boss->Update(0.0f);
	}

	if (settings_.enableBossIntroCamera && camera)
	{
		ApplyCameraLookAtBoss(camera, introCameraPosition_, introCameraTarget_);
	}

	if (t >= 1.0f)
	{
		ChangeState(settings_.enableBossIntroCamera ? State::FinishCutscene : State::Completed);
		if (state_ == State::Completed)
		{
			CompleteIntro(boss, camera);
		}
	}
}

void BossIntroController::UpdateCameraReturn(float deltaTime, GuardianBoss* boss, K4E::Camera* camera)
{
	stateTimer_ += deltaTime;
	const float t = settings_.cameraReturnTime <= 0.0f ? 1.0f : Clamp01(stateTimer_ / settings_.cameraReturnTime);

	if (camera)
	{
		const K4E::Vector3 cameraPosition = Lerp(introCameraPosition_, savedCameraPosition_, t);
		if (t < 1.0f)
		{
			ApplyCameraLookAtBoss(camera, cameraPosition, introCameraTarget_);
		}
		else
		{
			// 演出用カメラから通常カメラへ戻すため、保存していた位置と回転を直接復元する。
			camera->SetTranslate(savedCameraPosition_);
			camera->SetRotate(savedCameraRotation_);
			camera->Update();
		}
	}

	if (t >= 1.0f)
	{
		CompleteIntro(boss, camera);
	}
}

void BossIntroController::CompleteIntro(GuardianBoss* boss, K4E::Camera* camera)
{
	if (boss)
	{
		boss->ClearRootParentKeepingWorldPosition();
		// ボスを最終ワールド座標へ固定し、演出用カメラのWVPが残らないよう描画Transformも再同期する。
		boss->SetPosition(settings_.bossAppearPosition);
		boss->SetYaw(kPi);
		boss->ForceSyncWorldTransform();
	}

	// 登場完了時に大きめの土煙を一度だけ出し、地面を突き破って出た印象を作る。
	if (!bossIntroEndDustDone_)
	{
		EmitBossIntroDustBurst(settings_.dustEndBurstCount);
		bossIntroEndDustDone_ = true;
	}

	SetDebugSnapshot(boss, camera);
	// 登場完了後にボスAIを有効化するため、WorldへCollider登録と通常Update再開を通知する。
	bossColliderEnableRequested_ = true;
	hasPlayedBossIntro_ = true;
	ChangeState(State::Completed);

	const K4E::Vector3 bossPosition = boss ? boss->GetPosition() : settings_.bossAppearPosition;
	const K4E::Vector3 cameraPosition = camera ? camera->GetTranslate() : K4E::Vector3{};
	K4E::Log(
		"[BossIntro] Completed. boss=(" +
		std::to_string(bossPosition.x) + "," + std::to_string(bossPosition.y) + "," + std::to_string(bossPosition.z) +
		") camera=(" +
		std::to_string(cameraPosition.x) + "," + std::to_string(cameraPosition.y) + "," + std::to_string(cameraPosition.z) +
		") state=Completed active=false played=true\n");
}

void BossIntroController::UpdateBossIntroDust(float deltaTime, float riseT)
{
	if (!settings_.enableBossIntroDust)
	{
		return;
	}

	if (!bossIntroStartDustDone_)
	{
		// 上昇開始直後に強めの土煙を出し、地面が崩れ始めた印象を作る。
		EmitBossIntroDustBurst(settings_.dustStartBurstCount);
		bossIntroStartDustDone_ = true;
	}

	bossIntroDustEmitTimer_ += deltaTime;
	if (bossIntroDustEmitTimer_ >= settings_.dustEmitInterval && riseT < 1.0f)
	{
		bossIntroDustEmitTimer_ = 0.0f;
		bossEnemyVfx_.UpdateAppearDust(MakeBossIntroDustPosition(), settings_.dustLoopEmitCount);
	}
}

void BossIntroController::EmitBossIntroDustBurst(uint32_t emitCount)
{
	if (!settings_.enableBossIntroDust || emitCount == 0)
	{
		return;
	}

	bossEnemyVfx_.UpdateAppearDust(MakeBossIntroDustPosition(), emitCount);
}

K4E::Vector3 BossIntroController::MakeBossIntroDustPosition() const
{
	K4E::Vector3 dustPosition = settings_.bossAppearPosition;
	// BossSpawnPointはボス中心寄りなので、土煙は地面付近へ下げて発生させる。
	dustPosition.y = std::max(0.05f, dustPosition.y - settings_.dustGroundOffsetY);
	return dustPosition;
}

void BossIntroController::ResetBossIntroDustState()
{
	bossIntroDustEmitTimer_ = 0.0f;
	bossIntroStartDustDone_ = false;
	bossIntroEndDustDone_ = false;
}

void BossIntroController::ApplyCameraLookAtBoss(K4E::Camera* camera, const K4E::Vector3& cameraPosition, const K4E::Vector3& targetPosition) const
{
	if (!camera)
	{
		return;
	}

	K4E::Vector3 direction = targetPosition - cameraPosition;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f)
	{
		direction = { 0.0f, 0.0f, 1.0f };
	}

	// Camera::SetForwardで既存Cameraの回転を使い、急な切り替えではなく補間位置からボスへ向ける。
	camera->SetTranslate(cameraPosition);
	camera->SetForward(direction);
	camera->Update();
}

K4E::Vector3 BossIntroController::Lerp(const K4E::Vector3& a, const K4E::Vector3& b, float t)
{
	t = Clamp01(t);
	return a + (b - a) * t;
}

float BossIntroController::Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

const char* BossIntroController::ToStateLabel(State state)
{
	switch (state)
	{
	case State::WaitingAfterCrystalsBroken: return "WaitingAfterCrystalsBroken";
	case State::StartCutscene: return "StartCutscene";
	case State::CameraMoveToBoss: return "CameraMoveToBoss";
	case State::BossRising: return "BossRising";
	case State::FinishCutscene: return "FinishCutscene";
	case State::Completed: return "Completed";
	case State::None:
	default:
		return "None";
	}
}
