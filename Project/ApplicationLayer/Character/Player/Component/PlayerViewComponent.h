#pragma once
#include <functional>
#include <algorithm>

#include <BaseCharacter.h>   // WorldTransformEx / Vector3
#include "FpsCamera.h"
#include "PlayerInputSnapshot.h" // InputSnapshot
#include "Quaternion.h"

/// ---------- 前方宣言 ---------- ///
class Player;

/// -------------------------------------------------------------
///  プレイヤービュー（カメラ + 一人称メッシュ表示/構えポーズ）
/// -------------------------------------------------------------
class PlayerViewComponent
{
public: /// ---------- 構造体 ---------- ///

	// 描画フック：BaseCharacter の protected/public に依存しないため、Player からラムダで渡して使う
	struct FirstPersonRenderHooks
	{
		std::function<void(bool)> SetBodyActive;
		std::function<void(bool)> SetAllPartsActive;
		std::function<void(int, bool)> SetPartActive;
		std::function<int()> GetLeftArmIndex;
		std::function<int()> GetRightArmIndex;
	};

	/// ---------- カメラFOVフック ---------- ///
	struct CameraFovHooks
	{
		std::function<float()> GetFov;
		std::function<void(float)> SetFov;
	};

public: /// ---------- メンバ関数 ---------- ///

	void BindOwner(Player* owner) { owner_ = owner; }
	void BindBodyTransform(K4E::WorldTransformEx* bodyTr) { bodyTr_ = bodyTr; }
	void BindFirstPersonRenderHooks(FirstPersonRenderHooks hooks) { fpHooks_ = std::move(hooks); }

	void BindArmTransforms(K4E::WorldTransformEx* leftArm, K4E::WorldTransformEx* rightArm);
	void SetEmptyAimPose(const K4E::Vector3& leftLocalPos, const K4E::Vector3& rightLocalPos);

	void Initialize(Player* player);
	void UpdateLook(float dt, const InputSnapshot& input);
	void SyncToPlayer();
	void SyncViewModeToFirstPersonFlag();

	void SetFirstPersonView(bool enabled);
	bool IsFirstPersonView() const { return isFirstPersonView_; }

	void SetAiming(bool on);
	void SetReloadViewModelState(bool isReloading, float reloadTimer, float reloadDuration);

	float GetYaw() const { return fpsCamera_.GetYaw(); }
	K4E::Camera* GetCamera() { return fpsCamera_.GetCamera(); }

	void SetShootCamera(K4E::Camera* cam) { shootCamera_ = cam; }
	K4E::Camera* GetShootCamera() { return shootCamera_ ? shootCamera_ : fpsCamera_.GetCamera(); }

	K4E::WorldTransformEx* GetRightArmTransform() { return rightArmTr_; }

	void BindCameraFovHooks(CameraFovHooks hooks) { fovHooks_ = std::move(hooks); }
	void UpdateMovementFov(float deltaTime, bool isRunning, bool isBlinking, bool blinkJustStarted);

	void AddRecoil(float verticalDeg, float horizontalDeg);
	void DrawImGui();
	void SetWeaponAdsTuning(float adsFovDeg, float adsTransitionSpeed);
	void AddDamageFeedback(float strength01);

	void SetFirstPersonLeftArmVisible(bool visible);
	void StartMeleeSwing();
	bool IsMeleeSwingActive() const { return meleeSwingActive_; }

public:

	void StartDeathCamera(float targetPitchRad, float targetRollRad);
	void UpdateDeathCamera(float dt, float normalizedT);
	void ClearDeathCamera();

private:

	void ApplyFirstPersonRenderFlags();
	void UpdateFirstPersonArmPose(float dt);
	void UpdateRecoilViewModelKick(float dt);
	K4E::Quaternion MakeQuaternionFromEulerDeg(const K4E::Vector3& eulerDeg) const;

private: /// ---------- メンバ変数 ---------- ///

	Player* owner_ = nullptr;
	K4E::WorldTransformEx* bodyTr_ = nullptr;

	K4E::FpsCamera fpsCamera_{};
	bool isFirstPersonView_ = false;
	bool isAiming_ = false;

	K4E::Camera* shootCamera_ = nullptr;
	FirstPersonRenderHooks fpHooks_{};

	// ---- FP arms posing ----
	K4E::WorldTransformEx* leftArmTr_ = nullptr;
	K4E::WorldTransformEx* rightArmTr_ = nullptr;

	bool capturedBasePose_ = false;

	K4E::Vector3 baseLeftPos_{};
	K4E::Vector3 baseRightPos_{};

	// 通常構え。
	// 左腕は武器の中央へ添えるように内側へひねる。
	K4E::Vector3 baseLeftRot_ = { -1.04f, 2.35f, -0.61f };
	K4E::Vector3 baseRightRot_ = { -std::numbers::pi_v<float> *0.5f,0.0f,0.0f };

	// 通常時の左腕を武器側へ寄せるための位置補正。ImGuiから調整する。
	K4E::Vector3 leftHipSupportOffset_{ 0.63f, -0.16f, 0.40f };

	// ADS時は通常構えの向きを保ったまま、位置だけ少し中央・武器側へ寄せる。
	K4E::Vector3 aimLeftPos_{ -0.48f, 0.38f, 0.78f };
	K4E::Vector3 aimRightPos_{ 0.20f, 0.55f, 0.85f };

	// 左腕はADS中に大きく回転させない。通常構えのまま近づける。
	K4E::Vector3 aimRightRot_ = { -std::numbers::pi_v<float> *0.5f, 0.0f, 0.0f };
	K4E::Vector3 aimLeftRot_ = { -1.04f, 2.35f, -0.61f };

	float armBlend_ = 0.0f;
	float armBlendSpeed_ = 12.0f;

	float camPitch_ = 0.0f;

	// 上下視点で腕が画面を覆いすぎないよう、カメラPitch追従を弱める。
	// 以前の 1.0f はカメラ上下回転を腕へそのまま乗せていたため、上・下を向いた時に構えが崩れやすかった。
	float armPitchFollow_ = 0.28f;

	CameraFovHooks fovHooks_{};
	float prevAppliedFovMul_ = 1.0f;

	// ---- Movement FOV ----
	float runAlpha_ = 0.0f;
	float blinkAlpha_ = 0.0f;
	float blinkKick_ = 0.0f;

	float runFovAddDeg_ = 5.0f;
	float blinkFovAddDeg_ = 10.0f;
	float blinkKickAddDeg_ = 5.0f;

	float runInSpeed_ = 6.0f;
	float runOutSpeed_ = 4.0f;
	float blinkInSpeed_ = 20.0f;
	float blinkOutSpeed_ = 12.0f;
	float blinkKickOutSpeed_ = 22.0f;

	float adsSuppress_ = 0.85f;
	float minFovDeg_ = 35.0f;
	float maxFovDeg_ = 110.0f;

	// ---- 見た目反動（腕/武器のキック）----
	float vmKickPitch_ = 0.0f;
	float vmKickYaw_ = 0.0f;
	float vmKickRoll_ = 0.0f;
	float vmKickBack_ = 0.0f;
	float vmKickUp_ = 0.0f;
	bool  vmKickFlip_ = false;

	float vmKickRotReturnSpeed_ = 18.0f;
	float vmKickPosReturnSpeed_ = 0.90f;

	float vmKickPitchMul_ = 0.55f;
	float vmKickYawMul_ = 0.45f;
	float vmKickRollMul_ = 0.65f;
	float vmKickBackPerPitchDeg_ = 0.010f;
	float vmKickUpPerPitchDeg_ = 0.004f;

	float vmKickMaxPitchDeg_ = 12.0f;
	float vmKickMaxYawDeg_ = 8.0f;
	float vmKickMaxRollDeg_ = 10.0f;
	float vmKickMaxBack_ = 0.08f;
	float vmKickMaxUp_ = 0.04f;

	bool fpShowLeftArm_ = true;

	// ---- Reload view animation ----
	bool  reloadViewActive_ = false;
	float reloadViewTimer_ = 0.0f;
	float reloadViewDuration_ = 1.0f;
	float reloadAnimTimer_ = 0.0f;
	float reloadPoseAlpha_ = 0.0f;
	float reloadPoseBlendSpeed_ = 14.0f;
	float reloadEnterEndRate_ = 0.22f;
	float reloadHoldEndRate_ = 0.72f;
	float reloadReturnEndRate_ = 1.0f;

	// 保持中の「弾を入れる」ように見せる追加モーション。
	float reloadLoadStartRate_ = 0.36f;
	float reloadLoadPeakRate_ = 0.52f;
	float reloadLoadEndRate_ = 0.66f;

	// リロード時は腕と武器を少し下げ、前方視界を塞ぎすぎないようにする。
	K4E::Vector3 reloadWeaponDrop_{ 0.04f, -0.42f, -0.16f };
	K4E::Vector3 reloadWeaponRotDeg_{ 10.0f, -12.0f, 18.0f };
	K4E::Vector3 reloadRightArmRotDeg_{ -8.0f, 8.0f, -12.0f };
	K4E::Vector3 reloadLoadRightArmOffset_{ -0.02f, 0.00f, 0.05f };
	K4E::Vector3 reloadLoadRightArmRotDeg_{ 2.0f, -3.0f, 3.0f };

	// 左腕は少し後ろへ下げ、画面中央を塞ぎすぎない位置で装填動作を見せる。
	K4E::Vector3 reloadLeftArmOffset_{ 0.10f, -0.38f, -0.08f };
	K4E::Vector3 reloadLeftArmRotDeg_{ -4.0f, -8.0f, 10.0f };
	K4E::Vector3 reloadLoadLeftArmOffset_{ -0.04f, 0.01f, -0.16f };
	K4E::Vector3 reloadLoadLeftArmRotDeg_{ 3.0f, 4.0f, -6.0f };

	// ---- Melee view animation ----
	bool meleeSwingActive_ = false;
	float meleeSwingTimer_ = 0.0f;
	float meleeSwingDuration_ = 0.22f;

	float meleeSwingPitchDeg_ = 95.0f;
	float meleeSwingYawDeg_ = 18.0f;
	float meleeSwingRollDeg_ = 28.0f;
	float meleeSwingForward_ = 0.10f;
	float meleeSwingDown_ = 0.10f;
	float meleeSwingRight_ = 0.04f;

	// ---- ADS（Aim Down Sights） ----
	float weaponAdsFovDeg_ = 60.0f;
	float weaponAdsTransitionSpeed_ = 10.0f;

	float hipBaseFovDeg_ = 60.0f;
	bool  hipBaseFovCaptured_ = false;
	float adsFovAlpha_ = 0.0f;

	// ---- Damage camera feedback ----
	bool  dmgFlip_ = false;
	float dmgFovKickDeg_ = 0.0f;
	float dmgFovReturnSpeed_ = 28.0f;

	float dmgCamPitchDeg_ = 2.5f;
	float dmgCamYawDeg_ = 1.75f;
	float dmgFovKickAddDeg_ = 4.0f;
	float dmgFovKickMaxDeg_ = 8.0f;

	bool deathCameraActive_ = false;

	float deathCamPitchNow_ = 0.0f;
	float deathCamRollNow_ = 0.0f;

	float deathCamPitchTarget_ = 0.0f;
	float deathCamRollTarget_ = 0.0f;

	float deathCamBlendSpeed_ = 10.0f;
};
