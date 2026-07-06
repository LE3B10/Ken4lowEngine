#pragma once

#include "Vector3.h"
#include "BossEnemyVfx.h"

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	class Camera;
}

class GuardianBoss;

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// クリスタル全破壊後のボス登場演出を管理する。
///
/// GamePlayWorldから開始条件と実体だけを受け取り、遅延、カメラ移動、
/// ボス上昇、通常プレイ再開までの状態遷移をここへ集約する。
/// -------------------------------------------------------------
class BossIntroController
{
public:
	/// ボス登場演出の進行状態。
	enum class State
	{
		None,
		WaitingAfterCrystalsBroken,
		StartCutscene,
		CameraMoveToBoss,
		BossSpawnImpact,
		BossRising,
		FinishCutscene,
		Completed,
	};

	/// ParameterManagerで調整するボス登場演出パラメータ。
	struct Settings
	{
		float bossAppearDelay = 2.0f;
		K4E::Vector3 bossAppearPosition{ 0.0f, 2.25f, 30.0f };
		float bossStartOffsetY = -20.0f;
		float bossRiseTime = 3.0f;
		float cameraMoveTime = 1.5f;
		float cameraReturnTime = 1.0f;
		bool bossIntroPauseGame = true;
		bool enableBossIntroCamera = true;
		bool enableBossRiseEffect = true;
		bool enableBossIntroDust = true;
		bool enableBossIntroCameraShake = true;
		float dustGroundOffsetY = 2.2f;
		float dustEmitInterval = 0.08f;
		uint32_t dustStartBurstCount = 96;
		uint32_t dustLoopEmitCount = 10;
		uint32_t dustEndBurstCount = 128;
		float spawnShakeDuration = 0.75f;
		float spawnShakeAmplitude = 0.45f;
		float spawnShakeFrequency = 22.0f;
	};

	void Initialize(const K4E::Vector3& defaultBossPosition);
	void Finalize();

	void RequestStart(const K4E::Vector3& bossPosition);
	void Reset();
	void Update(float deltaTime, GuardianBoss* boss, K4E::Camera* camera);
	void SetDebugSnapshot(const GuardianBoss* boss, const K4E::Camera* camera);
	void DrawImGui();

	bool IsRunning() const { return state_ != State::None && state_ != State::Completed; }
	bool IsWaitingDelay() const { return state_ == State::WaitingAfterCrystalsBroken; }
	bool IsGameplayPaused() const;
	bool HasPlayed() const { return hasPlayedBossIntro_; }
	bool IsCompleted() const { return state_ == State::Completed; }
	bool ShouldRegisterBossCollider() const { return bossColliderEnableRequested_; }
	bool ConsumeBossSpawnRequest();
	bool ConsumeBossColliderEnableRequest();
	bool ConsumeDebugStartRequest();
	bool ConsumeDebugResetRequest();
	bool ConsumeDebugForceBossToAppearRequest();
	bool ConsumeDebugClearBossParentRequest();
	bool ConsumeDebugUseGameplayViewProjectionRequest();

	State GetState() const { return state_; }
	const Settings& GetSettings() const { return settings_; }
	const K4E::Vector3& GetBossAppearPosition() const { return settings_.bossAppearPosition; }
	K4E::Vector3 GetBossStartPosition() const;
	K4E::Vector3 GetBossLookTarget() const;

private:
	void RegisterParameters();
	void UnregisterParameters();
	void ApplyParameters();
	void ChangeState(State state);
	void BeginCutscene(K4E::Camera* camera);
	void UpdateCameraMove(float deltaTime, K4E::Camera* camera);
	void BeginBossSpawnImpact(K4E::Camera* camera);
	void UpdateBossRising(float deltaTime, GuardianBoss* boss, K4E::Camera* camera);
	void UpdateCameraReturn(float deltaTime, GuardianBoss* boss, K4E::Camera* camera);
	void CompleteIntro(GuardianBoss* boss, K4E::Camera* camera);
	void UpdateBossIntroDust(float deltaTime, float riseT);
	void EmitBossIntroDustBurst(uint32_t emitCount);
	void EmitBossIntroImpactBurst(uint32_t shockCount, uint32_t dustCount);
	K4E::Vector3 MakeBossIntroDustPosition() const;
	void ResetBossIntroDustState();
	void StartCameraShake(float duration, float amplitude, float frequency);
	void UpdateCameraShake(float deltaTime);
	K4E::Vector3 GetCameraShakeOffset() const;
	void ApplyCameraLookAtBoss(K4E::Camera* camera, const K4E::Vector3& cameraPosition, const K4E::Vector3& targetPosition) const;
	static K4E::Vector3 Lerp(const K4E::Vector3& a, const K4E::Vector3& b, float t);
	static float Clamp01(float value);
	static const char* ToStateLabel(State state);

private:
	static constexpr const char* kParameterGroupName = "BossIntro";

	Settings settings_{};
	State state_ = State::None;
	float stateTimer_ = 0.0f;
	bool hasPlayedBossIntro_ = false;
	bool bossSpawnRequested_ = false;
	bool bossColliderEnableRequested_ = false;
	bool debugStartRequested_ = false;
	bool debugResetRequested_ = false;
	bool debugForceBossToAppearRequested_ = false;
	bool debugClearBossParentRequested_ = false;
	bool debugUseGameplayViewProjectionRequested_ = false;
	K4E::Vector3 savedCameraPosition_{};
	K4E::Vector3 savedCameraRotation_{};
	K4E::Vector3 introCameraPosition_{ 0.0f, 9.0f, 18.0f };
	K4E::Vector3 introCameraTarget_{};
	K4E::Vector3 debugBossPosition_{};
	K4E::Vector3 debugBossLocalPosition_{};
	K4E::Vector3 debugBossWorldPosition_{};
	K4E::Vector3 debugCameraPosition_{};
	float debugBossCameraDistance_ = 0.0f;
	bool debugHasBoss_ = false;
	bool debugHasCamera_ = false;
	bool debugBossHasParent_ = false;
	const char* debugViewProjectionKind_ = "Unknown";
	BossEnemyVfx bossEnemyVfx_{};
	float bossIntroDustEmitTimer_ = 0.0f;
	bool bossIntroStartDustDone_ = false;
	bool bossIntroEndDustDone_ = false;
	bool bossSpawnImpactTriggered_ = false;
	float cameraShakeTimer_ = 0.0f;
	float cameraShakeDuration_ = 0.0f;
	float cameraShakeAmplitude_ = 0.0f;
	float cameraShakeFrequency_ = 0.0f;
	float cameraShakeSeed_ = 0.0f;
};
