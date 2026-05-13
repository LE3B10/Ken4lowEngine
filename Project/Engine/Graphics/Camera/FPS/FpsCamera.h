#pragma once
#include <random>
#include <Vector3.h>

struct InputSnapshot;
class Player;

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class Input;
	class Camera;

	/// -------------------------------------------------------------
	///					FPS視点専用カメラクラス
	/// -------------------------------------------------------------
	class FpsCamera
	{
	public: /// ---------- 列挙型 ---------- ///

		// 表示モード
		enum class ViewMode
		{
			FirstPerson, // 一人称視点
			ThirdBack,	 // 三人称視点（背後）
			ThirdFront	 // 三人称視点（正面）
		};

	public: // ---------- メンバ関数 ---------- //

		/// <summary>
		/// カメラの初期化処理を行います。<br/>
		/// ・Input シングルトンの取得<br/>
		/// ・Player との紐付け<br/>
		/// ・デフォルト Camera の取得と NearClip の設定<br/>
		/// を行います。
		/// </summary>
		/// <param name="player">視点の基準となるプレイヤーへのポインタ。</param>
		void Initialize(Player* player);

		// Look(角度)だけ更新（プレイヤー移動前に呼ぶ）
		void UpdateLook(const InputSnapshot& in, bool ignoreInput = false);

		// 位置同期＋行列更新（プレイヤー移動後に呼ぶ）
		void SyncToPlayer();

		// 便利関数（Look→Syncをまとめてやる）
		void Update(const InputSnapshot& in, bool ignoreInput = false);

		/// <summary>
		/// 毎フレームのカメラ更新処理。<br/>
		/// ・マウス入力から yaw / pitch を更新（ignoreInput が true の場合はスキップ）<br/>
		/// ・プレイヤーの頭位置を基準に視点位置を決定<br/>
		/// ・現在の ViewMode に応じて一人称 / 三人称の位置を計算<br/>
		/// ・Camera へ回転と位置を反映し、Update() を呼び出す<br/>
		/// といった処理を行います。<br/>
		/// Player がデバッグカメラ中の場合は何もしません。
		/// </summary>
		/// <param name="ignoreInput">true のとき、今フレームはマウス入力による回転更新を無視します。</param>
		void Update(bool ignoreInput = false);

		/// <summary>
		/// デバッグ用に「プレイヤー頭位置付近のカメラ領域」をワイヤーフレームで描画します。<br/>
		/// AABB を青色のワイヤーフレームで描画し、カメラ位置の確認に利用します。
		/// </summary>
		void DrawDebugCamera();

		/// <summary>
		/// ImGui によるデバッグ UI を描画します。<br/>
		/// ・ViewMode の切り替え<br/>
		/// ・Yaw / Pitch / 目線の高さ / TPS オフセット<br/>
		/// ・FOV / Near / Far の調整<br/>
		/// ・パラメータリセットボタン<br/>
		/// などを操作できます。（USE_IMGUI 定義時のみ有効）
		/// </summary>
		void DrawImGui();
		void DrawImGuiContent();

		/// <summary>
		/// カメラにリコイル（反動）を加えます。<br/>
		/// ・verticalAmount：上方向へのリコイル量（ピッチをマイナス方向へ）<br/>
		/// ・horizontalAmount：ランダムな左右ブレの最大値<br/>
		/// 実際の適用は Update 内で yaw_ / pitch_ に反映する想定です。
		/// </summary>
		/// <param name="verticalAmount">縦方向（ピッチ）に加えるリコイル量。</param>
		/// <param name="horizontalAmount">横方向（ヨー）のランダムブレの最大値。</param>
		void AddRecoil(float verticalAmount = 0.0f, float horizontalAmount = 0.0f);

		void SetDeathTilt(float pitchRad, float rollRad);

		void ClearDeathTilt();

		// AddRecoil() が UpdateLock() の後に呼ばれても「同フレームのSync」で反映されるように
		// cachedEuler_ を即座に再構築する。
		void RebuildCachedEulerAfterExternalChange();

		// イントロ終了直後など、外部カメラの向きをFPSカメラの初期向きへ同期する。
		void SetLookAngles(float pitchRad, float yawRad);

		/// <summary>
		/// ViewMode を一つずつ切り替えます。<br/>
		/// FirstPerson → ThirdBack → ThirdFront → FirstPerson → …<br/>
		/// のようにループします。（例：F5 キーが押されたときに呼ぶ）
		/// </summary>
		void CycleViewMode();

	public: // ---------- ゲッタ ---------- //

		/// <summary>
		/// 内部で使用している Camera を取得します。
		/// </summary>
		/// <returns>FpsCamera が操作している Camera インスタンス。</returns>
		Camera* GetCamera() const { return camera_; }

		/// <summary>
		/// 現在の yaw 角（ラジアン）を取得します。
		/// </summary>
		float GetYaw() const { return yaw_; }

		/// <summary>
		/// 現在の pitch 角（ラジアン）を取得します。
		/// </summary>
		float GetPitch() const { return pitch_; }

		/// <summary>
		/// 現在の表示モードを取得します。
		/// </summary>
		/// <returns>FirstPerson / ThirdBack / ThirdFront のいずれか。</returns>
		ViewMode GetViewMode() const { return viewMode_; }

		/// <summary>
		/// ADS中かどうか（入力の状態）を取得します。
		/// </summary>
		bool IsAiming() const { return isAiming_; }

		/// <summary>
		/// ADSブレンド値を取得します。0=腰だめ, 1=ADS（補間値）
		/// </summary>
		float GetAimAlpha() const { return aimAlpha_; }

	public: // ---------- セッタ ---------- //

		/// <summary>
		/// ADS（Aim Down Sights：覗き込み）状態フラグを設定します。<br/>
		/// true のときはマウス感度を下げるなどの処理に利用できます。
		/// </summary>
		/// <param name="isAiming">ADS 状態なら true。</param>
		void SetAiming(bool isAiming) { isAiming_ = isAiming; }

		/// <summary>
		/// ADS 時の感度補正係数を設定します。<br/>
		/// 例：0.5f なら通常時の半分の感度になります。
		/// </summary>
		/// <param name="factor">ADS 時に掛ける感度係数。</param>
		void SetAdsSensitivityFactor(float factor) { adsSensitivityFactor_ = factor; }

		/// <summary>
		/// 外部から Δt（1フレームの経過時間）をセットします。<br/>
		/// 歩行ボビングなどの時間ベース処理に利用します。
		/// </summary>
		/// <param name="deltaTime">前フレームからの経過時間（秒）。</param>
		void SetDeltaTime(float deltaTime) { deltaTime_ = deltaTime; }

		/// <summary>
		/// 表示モードを直接設定します。
		/// </summary>
		/// <param name="m">設定したい ViewMode。</param>
		void SetViewMode(ViewMode m) { viewMode_ = m; }

	private: /// ---------- メンバ関数 ---------- ///

		// プレイヤーの状態に基づいてカメラ位置を計算し、Camera に反映します。
		void RebuildCachedEuler();

	private: // ---------- メンバ ---------- //

		Input* input_ = nullptr;
		Camera* camera_ = nullptr;
		Player* player_ = nullptr;

		// 視点角度（ラジアン）
		float yaw_ = 0.0f;
		float pitch_ = 0.0f;

		// 前フレームの回転（IdleSway用）
		Vector3 cachedEuler_ = { 0.0f, 0.0f, 0.0f };
		bool applyIdleSwayThisFrame_ = false;

		// 感度
		const float mouseSensitivity_ = 0.002f;
		const float controllerSensitivity_ = 0.05f;

		// ピッチ制限
		const float minPitch_ = -1.5f;
		const float maxPitch_ = +1.5f;

		// カメラ高さオフセット（頭位置）
		float eyeHeight_ = 1.5f;

		// Aiming状態フラグ
		bool isAiming_ = false;
		float adsSensitivityFactor_ = 0.25f;

		// ADSブレンド（0=腰だめ,1=ADS）と切替速度
		float aimAlpha_ = 0.0f;
		float adsInSpeed_ = 18.0f;   // ADSに入る速さ（大きいほど速い）
		float adsOutSpeed_ = 14.0f;   // ADSから戻る速さ

		// FOV（度）
		float hipFovDeg_ = 60.0f;
		float adsFovDeg_ = 45.0f;

		// ADS中は揺れを弱くする（1=そのまま、0=止める）
		float adsSwayScale_ = 0.25f;

		// ボビング処理用
		float currentBobbingSpeed_ = 0.0f;
		float currentBobbingAmplitude_ = 0.0f;
		float bobbingTimer_ = 0.0f;
		float bobbingAmplitude_ = 0.25f;
		float bobbingSpeed_ = 10.0f;
		float deltaTime_ = 1.0f / 60.0f;

		// しゃがむ状態のフラグ
		bool isCrouching_ = false;
		const float standEyeHeight_ = 1.685f;
		const float crouchEyeHeight_ = 1.2f;

		bool wasGrounded_ = true;

		// 着地バウンド
		float landingBounceTimer_ = 0.0f;
		const float landingBounceDuration_ = 0.25f;
		const float landingBounceAmplitude_ = 0.25f;

		// リコイル（後で使う）
		float recoilOffsetPitch_ = 0.0f;
		float recoilOffsetYaw_ = 0.0f;

		// リコイル設定（まずは固定値）
		bool  recoilEnabled_ = true;
		float recoilReturnSpeed_ = 22.0f;   // 戻る速さ（大きいほど早く戻る）
		float recoilMaxPitchDeg_ = 12.0f;   // 縦反動の上限（度）
		float recoilMaxYawDeg_ = 6.0f;    // 横ブレの上限（度）

		// ---------- Idle sway (撃っていない時の微揺れ) ----------
		bool idleSwayEnabled_ = true;
		bool idleSwayApplyInThird_ = false; // TPSにも適用したいなら true
		float idleSwayTimer_ = 0.0f;

		// 回転揺れ（度で管理：Update内でラジアンへ変換）
		float idleSwayAmpYawDeg_ = 0.25f;   // 左右
		float idleSwayAmpPitchDeg_ = 0.18f; // 上下
		float idleSwayFreqYaw1_ = 1.2f;
		float idleSwayFreqYaw2_ = 2.1f;
		float idleSwayFreqPit1_ = 1.6f;
		float idleSwayFreqPit2_ = 2.7f;

		// 位置揺れ（メートル）
		float idleSwayPosX_ = 0.006f; // 左右
		float idleSwayPosY_ = 0.008f; // 呼吸上下
		float idleSwayPosZ_ = 0.004f; // 前後
		float idleSwayFreqPos1_ = 1.0f;
		float idleSwayFreqPos2_ = 2.3f;

		// 追従（大きいほど即座に追従）
		float idleSwaySmooth_ = 10.0f;

		// 内部：スムーズ後のオフセット（ラジアン / メートル）
		float idleSwayYawRad_ = 0.0f;
		float idleSwayPitchRad_ = 0.0f;
		Vector3 idleSwayPosOffset_ = { 0.0f, 0.0f, 0.0f };

		std::default_random_engine randomEngine_;

		bool debugThirdPerson_ = false;
		float debugCamDistance_ = 3.0f;
		float debugShoulderHeight_ = 1.6f;
		float debugSideOffset_ = 0.0f;

		ViewMode viewMode_ = ViewMode::FirstPerson;

		// TPS用オフセット
		float tpsDistance_ = 20.0f;
		float tpsForward_ = 20.0f;
		float tpsUpOffset_ = 0.15f;

		float deathTiltPitch_ = 0.0f;
		float deathTiltRoll_ = 0.0f;
	};

} // namespace Ken4lowEngine
