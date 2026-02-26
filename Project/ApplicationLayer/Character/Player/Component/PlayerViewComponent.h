#pragma once
#include <functional>
#include <algorithm>

#include <BaseCharacter.h>   // WorldTransformEx / Vector3
#include "FpsCamera.h"
#include "PlayerInputSnapshot.h" // InputSnapshot

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
		// BaseCharacter の protected/public に依存しないため、Player からラムダで渡して使う
		std::function<void(bool)> SetBodyActive;
		std::function<void(bool)> SetAllPartsActive;
		std::function<void(int, bool)> SetPartActive;
		std::function<int()> GetLeftArmIndex;
		std::function<int()> GetRightArmIndex;
	};

	/// ---------- カメラFOVフック ---------- ///
	struct CameraFovHooks
	{
		std::function<float()> GetFov;        // 現在FOV（単位は何でもOK）
		std::function<void(float)> SetFov;    // 設定FOV（同じ単位）
	};

public: /// ---------- メンバ関数 ---------- ///

	void BindOwner(Player* owner) { owner_ = owner; }
	void BindBodyTransform(K4E::WorldTransformEx* bodyTr) { bodyTr_ = bodyTr; }
	void BindFirstPersonRenderHooks(FirstPersonRenderHooks hooks) { fpHooks_ = std::move(hooks); }

	// ---- Arms（見た目）----
	// BaseCharacter の parts_ から、腕の Transform を渡す（ローカル translate_/rotate_ を上書きして“構え”を作る）
	void BindArmTransforms(K4E::WorldTransformEx* leftArm, K4E::WorldTransformEx* rightArm);

	// “武器なしでも構えている” 用のターゲットポーズを調整したい場合
	// （local座標：+Zが前、+Yが上、+Xが右想定）
	void SetEmptyAimPose(const K4E::Vector3& leftLocalPos, const K4E::Vector3& rightLocalPos);

	// 初期化（Player::Initialize から呼ぶ）
	void Initialize(Player* player);

	// Update前半：入力に基づくカメラ回転（Yaw/Pitch 等）
	// ※この後の移動シミュレーションの “基準Yaw” として使う
	void UpdateLook(float dt, const InputSnapshot& input);

	// Update後半：移動後にカメラをプレイヤーへ同期
	void SyncToPlayer();

	// 画面モードに応じて FirstPerson 表示フラグを同期
	void SyncViewModeToFirstPersonFlag();

	// 外部から明示的に切替したい場合（従来Player::SetFirstPersonView 互換）
	void SetFirstPersonView(bool enabled);
	bool IsFirstPersonView() const { return isFirstPersonView_; }

	// Aim（ADS）状態：FpsCamera の演出（揺れ抑制/視野など） + 腕の“構え”ブレンド
	void SetAiming(bool on);

	// カメラ情報（射撃/デバッグ用）
	float GetYaw() const { return fpsCamera_.GetYaw(); }
	K4E::Camera* GetCamera() { return fpsCamera_.GetCamera(); }

	// 発射に使うカメラ（外部差し替え可能。未指定なら FpsCamera のカメラ）
	void SetShootCamera(K4E::Camera* cam) { shootCamera_ = cam; }
	K4E::Camera* GetShootCamera() { return shootCamera_ ? shootCamera_ : fpsCamera_.GetCamera(); }

	// 将来「武器を右腕へ親子付け」する時に使う（右腕Transformへの参照）
	K4E::WorldTransformEx* GetRightArmTransform() { return rightArmTr_; }

	void BindCameraFovHooks(CameraFovHooks hooks) { fovHooks_ = std::move(hooks); }

	void UpdateMovementFov(float deltaTime, bool isRunning, bool isDashing, bool dashJustStarted);

	// リコイル処理
	void AddRecoil(float verticalDeg, float horizontalDeg);

	// Debug UI
	void DrawImGui() { fpsCamera_.DrawImGui(); }

	void SetWeaponAdsTuning(float adsFovDeg, float adsTransitionSpeed);

private:

	void ApplyFirstPersonRenderFlags();
	void UpdateFirstPersonArmPose(float dt);
	void UpdateRecoilViewModelKick(float dt);

private: /// ---------- メンバ変数 ---------- ///

	// 所有プレイヤーへの弱参照
	Player* owner_ = nullptr;

	// BaseCharacter の body_ の Transform への参照
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
	K4E::Vector3 baseLeftRot_ = { -std::numbers::pi_v<float> *0.5f,0.0f,0.0f };
	K4E::Vector3 baseRightRot_ = { -std::numbers::pi_v<float> *0.5f,0.0f,0.0f };

	// “空手構え” 目標ポーズ（local）
	K4E::Vector3 aimLeftPos_{ -0.10f, 0.55f, 0.90f };
	K4E::Vector3 aimRightPos_{ 0.35f, 0.55f, 0.85f };

	K4E::Vector3 aimRightRot_ = { -std::numbers::pi_v<float> *0.5f,  0.20f, 0.0f };
	K4E::Vector3 aimLeftRot_ = { -std::numbers::pi_v<float> *0.5f, -0.1f,  0.0f };

	float armBlend_ = 0.0f;        // 0=relax, 1=aim
	float armBlendSpeed_ = 12.0f;  // ブレンド速度

	// camera pitch cache
	float camPitch_ = 0.0f;
	float armPitchFollow_ = 1.0f;

	// PlayerViewComponent.h の private メンバに追加
	CameraFovHooks fovHooks_{};

	// 前フレームで自分が掛けた倍率（基準FOVを復元するため）
	float prevAppliedFovMul_ = 1.0f;

	// ---- Movement FOV ----
	float runAlpha_ = 0.0f;
	float dashAlpha_ = 0.0f;
	float dashKick_ = 0.0f;

	// 追加するFOV（度）
	float runFovAddDeg_ = 5.0f;      // 走行中 +5°
	float dashFovAddDeg_ = 10.0f;     // ダッシュ中 +10°
	float dashKickAddDeg_ = 5.0f;    // ダッシュ開始瞬間 +5°

	// ブレンド速度
	float runInSpeed_ = 6.0f;
	float runOutSpeed_ = 4.0f;
	float dashInSpeed_ = 20.0f;
	float dashOutSpeed_ = 12.0f;
	float dashKickOutSpeed_ = 22.0f;

	// ADS中は効果を弱める（0..1）
	float adsSuppress_ = 0.85f; // aimAlpha=1 のとき (1-0.85)=0.15倍にする
	float minFovDeg_ = 35.0f;
	float maxFovDeg_ = 110.0f;

	// ---- 見た目反動（腕/武器のキック）----
	// ※プレイヤーのroot/body本体は衝突判定や移動に影響するため触らず、
	// 一人称で見える腕（=上半身の見た目）にだけ反動を乗せる。
	float vmKickPitch_ = 0.0f;  // rad
	float vmKickYaw_ = 0.0f;    // rad
	float vmKickRoll_ = 0.0f;   // rad
	float vmKickBack_ = 0.0f;   // local -Z 方向（m相当）
	float vmKickUp_ = 0.0f;     // local +Y 方向（m相当）
	bool  vmKickFlip_ = false;  // 左右揺れを交互にする

	float vmKickRotReturnSpeed_ = 18.0f; // rad/s 相当（Approach）
	float vmKickPosReturnSpeed_ = 0.90f; // m/s   相当（Approach）

	float vmKickPitchMul_ = 0.55f;       // camera反動(度)→腕ピッチ(度)の倍率
	float vmKickYawMul_ = 0.45f;         // camera横反動(度)→腕ヨー(度)の倍率
	float vmKickRollMul_ = 0.65f;        // camera横反動(度)→腕ロール(度)の倍率
	float vmKickBackPerPitchDeg_ = 0.010f; // 1度あたり後退量
	float vmKickUpPerPitchDeg_ = 0.004f;   // 1度あたり上方向量

	float vmKickMaxPitchDeg_ = 12.0f;
	float vmKickMaxYawDeg_ = 8.0f;
	float vmKickMaxRollDeg_ = 10.0f;
	float vmKickMaxBack_ = 0.08f;
	float vmKickMaxUp_ = 0.04f;

	// ---- ADS（Aim Down Sights） ----
	float weaponAdsFovDeg_ = 60.0f;
	float weaponAdsTransitionSpeed_ = 10.0f;

	// ---- ADS FOV合成用 ----
	float hipBaseFovDeg_ = 60.0f;     // 腰だめ基準FOV（度）
	bool  hipBaseFovCaptured_ = false; // 初回だけ現在FOVを基準として拾う
	float adsFovAlpha_ = 0.0f;         // 武器ADS FOV用の補間(0..1)
};
