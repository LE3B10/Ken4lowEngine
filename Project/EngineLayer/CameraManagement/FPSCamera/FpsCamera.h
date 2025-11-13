#pragma once
#include <Quaternion.h>
#include <random>

/// ---------- 前方宣言 ---------- ///
class Player;
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

	/// <summary>
	/// カメラにリコイル（反動）を加えます。<br/>
	/// ・verticalAmount：上方向へのリコイル量（ピッチをマイナス方向へ）<br/>
	/// ・horizontalAmount：ランダムな左右ブレの最大値<br/>
	/// 実際の適用は Update 内で yaw_ / pitch_ に反映する想定です。
	/// </summary>
	/// <param name="verticalAmount">縦方向（ピッチ）に加えるリコイル量。</param>
	/// <param name="horizontalAmount">横方向（ヨー）のランダムブレの最大値。</param>
	void AddRecoil(float verticalAmount = 0.0f, float horizontalAmount = 0.0f);

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

private: // ---------- メンバ ---------- //

	Input* input_ = nullptr;
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;

	// 視点角度（ラジアン）
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;

	// 感度
	const float mouseSensitivity_ = 0.002f; // マウス感度（例: 0.002f）
	const float controllerSensitivity_ = 0.05f; // コントローラー感度（例: 0.05f）

	// ピッチ制限
	const float minPitch_ = -1.5f; // 下限
	const float maxPitch_ = +1.5f; // 上限

	// カメラ高さオフセット（頭位置）
	float eyeHeight_ = 1.5f;

	// Aiming状態フラグ
	bool isAiming_ = false;
	// ADS状態の感度補正係数（例: 0.5で半分の感度）
	float adsSensitivityFactor_ = 0.25f;

	// ボビング処理用
	float currentBobbingSpeed_ = 0.0f;
	float currentBobbingAmplitude_ = 0.0f;
	float bobbingTimer_ = 0.0f;
	float bobbingAmplitude_ = 0.25f;   // 振れ幅
	float bobbingSpeed_ = 10.0f;       // サイクル速度
	float deltaTime_ = 1.0f / 60.0f;    // 仮：外部から渡すべき

	// しゃがむ状態のフラグ
	bool isCrouching_ = false; // しゃがむ状態のフラグ
	const float standEyeHeight_ = 1.685f; // 立ち上がり時の目の高さ
	const float crouchEyeHeight_ = 1.2f; // しゃがみ時の目の高さ

	// 着地検出用（前フレームとの比較）
	bool wasGrounded_ = true;

	// 着地バウンド処理用
	float landingBounceTimer_ = 0.0f;
	const float landingBounceDuration_ = 0.25f; // バウンドの持続時間
	const float landingBounceAmplitude_ = 0.25f; // バウンドの深さ

	float recoilOffsetPitch_ = 0.0f;
	float recoilOffsetYaw_ = 0.0f;
	std::default_random_engine randomEngine_;

	bool debugThirdPerson_ = false;   // true のとき TPS 表示
	// TPS オフセット（好みに応じて調整）
	float debugCamDistance_ = 3.0f; // 背後距離
	float debugShoulderHeight_ = 1.6f; // 肩の高さ
	float debugSideOffset_ = 0.0f;// 右肩越し (+左なら負に)

	ViewMode viewMode_ = ViewMode::FirstPerson;

	// TPS用オフセット
	float tpsDistance_ = 20.0f;     // 後方へ下げる距離
	float tpsForward_ = 20.0f;     // 前方へ出す距離（ThirdFront 用）
	float tpsUpOffset_ = 0.15f;    // 少し上げる微調整
};
