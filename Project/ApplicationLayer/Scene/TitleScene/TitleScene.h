#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <BaseScene.h>
#include "SkyBox.h"

#include "ConfirmQuitOverlay.h"
#include "ITitleSceneState.h"

#include <memory>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;
class Camera;
class SceneManager;

/// -------------------------------------------------------------
///					　ゲームタイトルシーンクラス
/// -------------------------------------------------------------
class TitleScene : public BaseScene
{
private: /// ---------- 構造体 ---------- ///

	/// ---------- カメラの姿勢スナップショット ---------- ///
	struct Pose
	{
		Vector3 position; // 位置
		float yaw;		// Y軸回りの回転角
		float pitch;	// X軸回りの回転角
	};

	/// ---------- カメラの軌道状態 ---------- ///
	struct OrbitState
	{
		Vector3 center = { 0.0f, 10.0f, 0.0f }; // 注視点
		float radius = 10.0f;					// 半径
		float speed = 0.0625f;					// 速度
		float angle = 0.0f;						// 角度
		float lastYaw = 0.0f;					// 最後のY軸回り角度（補間用）
		float lastPitch = -0.10f;				// 最後のX軸回り角度（補間用）
	};

	/// ---------- ロビーでのカメラスイング状態 ---------- ///
	struct LobbySwing
	{
		float radius = 0.0f;     // 半径
		float height = 0.0f;     // 高さ
		float baseTheta = 0.0f;  // 基準角度
		float basePitch = 0.0f;  // 基準ピッチ角度
		float phase = 0.0f;      // 進行フェーズ
		float speed = 0.0625f;   // 速度
		float amplitude = 0.03f; // 振幅
		Vector3 lookAt = { std::numbers::pi_v<float>, 6.0f, 0.0f }; // 注視点のオフセット角度（Y軸回り、X軸回り、Z軸回り）
		Vector3 cameraPosition = { 0.0f, 3.0f, -12.0f };			// カメラのオフセット位置
	};

	/// ---------- タイトル・ロゴ＆ヒント ---------- ///
	struct LogoUI
	{
		std::unique_ptr<Sprite> logoSprite; // ロゴスプライト
		float alpha = 0.0f;                  // アルファ値
		float scale = 1.0f;                  // スケール
		Vector2 baseSize = { 0.0f, 0.0f };    // 基準サイズ（Initializeで取得）

		// サブ : 入場タイミング
		float showDelay = 0.5f;    // 表示遅延時間
		float showLeft = 0.0f;     // 残り時間
		float exitFade = 0.25f;    // 退場フェード時間
		float exitLeft = 0.0f;    // 退場残り時間
	};

	/// ---------- クリックヒント ---------- ///
	struct ClickHintUI
	{
		std::unique_ptr<Sprite> hintSprite; // ヒントスプライト
		bool isVisible = false;             // 表示フラグ
		float phase = 0.0f;                 // 点滅/ゆれ用
		Vector2 offset = { 0.0f, 40.0f };   // ロゴの少し下
		Vector2 baseSize = { 0.0f, 0.0f };  // 基準サイズ（Initializeで取得）
		float marginY = 16.0f;              // ロゴの下マージン(px)
		float wobblePx = 3.0f;              // 上下ゆれ振幅(px)
		float pulseMag = 0.06f;             // スケール脈動(=±6%)
		float blinkMin = 0.55f;             // 最小アルファ

		// 押下・ホバー用
		bool isPressing = false; // ヒント内で押し始めたか
		float pressAnim = 0.0f;  // 0..1 押し込みアニメ
		float hoverAnim = 0.0f;  // 0..1 ホバーアニメ

		// 押下・ホバー時の見た目（ボタンと同じ思想）
		const float scalePress = 0.05f;    // 押下で-5%縮む
		const float scaleHover = 0.04f;    // ホバーで+4%拡大
		const float offsetPressY = 3.0f;   // 押下で3px沈む
	};

	/// ---------- バトルへボタンUI ---------- ///
	struct BattleButtonUI
	{
		std::unique_ptr<Sprite> btnSprite;	   // ボタンスプライト
		std::unique_ptr<Sprite> btnShadow;	   // 影スプライト
		Vector2 position = { 640.0f, 600.0f }; // 配置座標（画面中央下）
		Vector2 size = { 420.0f, 140.0f };     // 表示サイズ
		Vector2 anchor = { 0.5f, 0.5f };	   // アンカー（中心）
		bool isPressing = false;               // ボタン内で押し始めたか
		float pressAnim = 0.0f;                // 0=通常, 1=押し込み
		float hoverAnim = 0.0f;                // 0=非ホバー, 1=ホバー
		const float pressOffsetPx = 6.0f;      // 押下で沈む距離(px)
		const float scalePress = 0.04f;        // 押下で縮む率(=4%)
		const float scaleHover = 0.03f;        // ホバーで大きくなる率(=3%)
	};

	/// ---------- タイマー群 ---------- ///
	struct Timers
	{
		float time = 0.0f;				   // シーン開始からの経過時間
		float duration = 1.0f;			   // シーン全体の継続時間
		float state = 0.0f;				   // 状態遷移用
		float idle = 0.0f;				   // 無操作でタイトルに
		float returnSeconds = 15.0f;	   // 無操作でタイトルに戻るまでの時間
		float minTitleSeconds = 1.0f;      // タイトルの最小表示時間
		float afterReturnCooldown = 0.75f; // タイトルへ戻った直後の入力クールダウン
		float inputCooldownLeft = 0.0f;    // 現在のクールダウン残り
	};

public: /// ---------- 列挙型 ---------- ///

	// シーンの状態を管理する列挙型
	enum class State
	{
		TitleAttract,	   // タイトルアトラクトモード
		TransitionToLobby, // ロビーへの遷移
		LobbyIdle,		   // ロビーでの待機
		ToTitle,		   // 操作時間が無かったらタイトルへ戻る
		Loading,		   // ロード中
		SettingUp,		   // セットアップ中
	};
	State state_ = State::TitleAttract; // 現在のシーン状態

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// カメラの初期化
	void InitializeCamera();

	// タイトルロゴの初期化
	void InitializeLogoUI();

	// バトルへボタンUIの初期化
	void InitializeBattleButtonUI();

	// 影のスプライトの初期化
	void InitializeButtonShadowSprite();

	// クリックヒントUIの初期化
	void InitializeClickHintUI();

public: /// ---------- セッタ ---------- ///

	// シーン状態を変更
	void ChangeState(std::unique_ptr<ITitleSceneState> newState);

	// 状態をセット
	void SetState(State s) { state_ = s; }

public: /// ---------- ゲッタ ---------- ///

	// シーンマネージャーを取得
	SceneManager* GetSceneManager() const { return sceneManager_; }

	// DirectX共通管理を取得
	DirectXCommon* GetDirectXCommon() const { return dxCommon_; }

	// 操作入力を取得
	Input* GetInput()  const { return input_; }

	// メインカメラを取得
	Camera* GetCamera() { return EnsureCamera(); } // 必要ならデフォルト取得

	// ロゴスプライトを取得
	Sprite* GetLogoSprite() const { return logoSprite_.get(); }

	// 現在のシーン状態を取得
	State GetState() const { return state_; }

	// カメラの軌道状態を取得
	OrbitState& GetOrbitState() { return orbitState_; }

	// ロゴ＆ヒントUI状態を取得
	LogoUI& GetLogoUI() { return logoUI_; }

	// クリックヒントUI状態を取得
	ClickHintUI& GetClickHintUI() { return clickHintUI_; }

	// バトルへボタンUI状態を取得
	BattleButtonUI& GetBattleButtonUI() { return battleButtonUI_; }

	// 各種タイマー状態を取得
	Timers& GetTimers() { return timers_; }

	// ロビーでのカメラスイング状態を取得
	LobbySwing& GetLobbySwing() { return lobbySwing_; }

	// カメラの遷移元姿勢を取得
	Pose& GetPoseFrom() { return poseFrom_; }

	// カメラの遷移先姿勢を取得
	Pose& GetPoseTo() { return poseTo_; }

private: /// ---------- メンバ関数 ---------- ///

	// Debug用更新処理
	void UpdateDebug();
	Camera* EnsureCamera();

private: /// ---------- メンバ変数 ---------- ///

	Pose poseFrom_{}, poseTo_{};		 // 遷移元・先のカメラ姿勢
	OrbitState orbitState_ = {};		 // カメラの軌道状態
	LobbySwing lobbySwing_ = {};		 // ロビーでのカメラスイング状態
	LogoUI logoUI_ = {};				 // ロゴ＆ヒントUI状態
	ClickHintUI clickHintUI_ = {};		 // クリックヒントUI状態
	BattleButtonUI battleButtonUI_ = {}; // バトルへボタンUI状態
	Timers timers_ = {};				 // 各種タイマー

	DirectXCommon* dxCommon_ = nullptr; // DirectX共通管理
	Input* input_ = nullptr;			// 入力管理
	Camera* camera_ = nullptr;			// メインカメラ

	bool isDebugCamera_ = false; // デバッグモード

	std::unique_ptr<SkyBox> skyBox_;	// スカイボックス
	std::unique_ptr<Object3D> terrain_; // 地形オブジェクト

	bool requestChange_ = false; // シーン切り替え要求
	float timer_ = 0.0f;   // シーン開始からの経過時間

	// タイトル用ロゴ（タイトル時だけ描画）
	std::unique_ptr<Sprite> logoSprite_;

	// ボタン影スプライト
	std::unique_ptr<ConfirmQuitOverlay> quitOverlay_;

	// 現在のシーン状態
	std::unique_ptr<ITitleSceneState> currentState_ = nullptr;
};

