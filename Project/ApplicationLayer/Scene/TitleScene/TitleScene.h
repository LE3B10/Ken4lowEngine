#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <BaseScene.h>
#include "SkyBox.h"
#include <FadeController.h>

#include <memory>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;
class Camera;

/// -------------------------------------------------------------
///					　ゲームタイトルシーンクラス
/// -------------------------------------------------------------
class TitleScene : public BaseScene
{
private: /// ---------- 構造体 ---------- ///

	// カメラの姿勢スナップショット
	struct Pose
	{
		Vector3 position; // 位置
		float yaw;		// Y軸回りの回転角
		float pitch;	// X軸回りの回転角
	};
	Pose poseFrom_{}, poseTo_{};

private: /// ---------- 列挙型 ---------- ///

	// シーンの状態を管理する列挙型
	enum class State
	{
		TitleAttract,	   // タイトルアトラクトモード
		TransitionToLobby, // ロビーへの遷移
		LobbyIdle,		   // ロビーでの待機
		ToTitle,		   // 操作時間が無かったらタイトルへ戻る
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

	// Debug用更新処理
	void UpdateDebug();

	// 状態ごとの更新処理
	void UpdateTitleAttract(float dt);	    // タイトルアトラクトモードの
	void UpdateTransitionToLobby(float dt); // ロビーへの遷移
	void UpdateLobbyIdle(float dt);		    // ロビーでの待機
	void UpdateToTitle(float dt);		    // 操作時間が無かったらタイトルへ戻

	Camera* EnsureCamera();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;

	bool isDebugCamera_ = false; // デバッグモード

	std::unique_ptr<SkyBox> skyBox_ = nullptr;

	std::unique_ptr<Object3D> object3D_ = nullptr;
	std::unique_ptr<Object3D> terrain_ = nullptr;

	// タイトル用ロゴ（タイトル時だけ描画）
	std::unique_ptr<Sprite> logoSprite_;

	// --- ロビーHUD（見た目だけのプレースホルダ） ---
	std::unique_ptr<Sprite> iconGear_;    // 右上：設定
	std::unique_ptr<Sprite> iconCoin_;    // 右上：コインアイコン
	std::unique_ptr<Sprite> btnShop_;     // 左下：ショップ
	std::unique_ptr<Sprite> xpBack_;      // 左上：XPバー 背景
	std::unique_ptr<Sprite> xpFill_;      // 左上：XPバー 充填

	std::unique_ptr<Sprite> btnBattle_;     // 「バトルへ」ボタン
	// 影用スプライト
	std::unique_ptr<Sprite> btnBattleShadow_;

	std::unique_ptr<Sprite> clickHint_;
	bool  clickHintVisible_ = false;
	float clickHintPhase_ = 0.0f;   // 点滅/ゆれ用
	Vector2 clickHintOffset_{ 0.0f, 40.0f }; // ロゴの少し下
	Vector2 clickHintBaseSize_{ 0, 0 };       // 基準サイズ（Initializeで取得）
	float  clickHintMarginY_ = 16.0f;        // ロゴの下マージン(px)
	float  clickHintWobblePx_ = 3.0f;         // 上下ゆれ振幅(px)
	float  clickHintPulseMag_ = 0.06f;        // スケール脈動(=±6%)
	float  clickHintBlinkMin_ = 0.55f;        // 最小アルファ
	// 既存の clickHint_ まわりの直後に追加
	bool  clickHintPressing_ = false; // ヒント内で押し始めたか
	float clickHintPressAnim_ = 0.0f;  // 0..1 押し込みアニメ
	float clickHintHoverAnim_ = 0.0f;  // 0..1 ホバーアニメ

	// 押下・ホバー時の見た目（ボタンと同じ思想）
	const float clickHintScalePress_ = 0.05f; // 押下で-5%縮む
	const float clickHintScaleHover_ = 0.04f; // ホバーで+4%拡大
	const float clickHintOffsetPressY_ = 3.0f; // 押下で3px沈む

	// クリック当たり判定用（UI座標）
	Vector2 battleBtnPos_{ 640.0f, 600.0f };     // 配置座標（画面中央下）
	Vector2 battleBtnSize_{ 420.0f, 140.0f };    // 表示サイズ
	Vector2 battleBtnAnchor_{ 0.5f, 0.5f };      // アンカー（中心）

	// 押下・ホバー状態
	bool   battleBtnPressing_ = false; // ボタン内で押し始めたか
	float  battlePressAnim_ = 0.0f;  // 0=通常, 1=押し込み
	float  battleHoverAnim_ = 0.0f;  // 0=非ホバー, 1=ホバー

	// 見た目パラメータ
	const float battlePressOffsetPx_ = 6.0f;  // 押下で沈む距離(px)
	const float battleScalePress_ = 0.04f; // 押下で縮む率(=4%)
	const float battleScaleHover_ = 0.03f; // ホバーで大きくなる率(=3%)

private: /// ---------- 定数 ---------- ///

	// --- 状態用タイマーと無操作復帰 ---
	float stateTimer_ = 0.0f;     // 状態遷移用
	float idleTimer_ = 0.0f;	  // 無操作でタイトルに
	const float returnSeconds_ = 75.0f; // 無操作でタイトルに戻るまでの時間 

	// 現在のカメラ姿勢
	float lastYaw_ = 0.0f;
	float lastPitch_ = -0.10f;

	// --- ロビーの構図：位置と注視点 ---
	Vector3 lobbyPosition_ = { 0.0f, 3.0f, -12.0f }; // カメラ位置
	Vector3 lobbyLookAt_ = { std::numbers::pi_v<float>, 6.0f, 0.0f };	  // カメラ注視点

	// --- 遷移用の姿勢スナップショット ---
	float transTime_ = 0.0f;	 // 遷移時間
	float transDuration_ = 1.0f; // 遷移にかける時間

	// --- ロビー中の“左右スイング”用（水平のみ） ---
	float lobbyRadius_ = 0.0f;   // lookAt からの水平距離
	float lobbyHeight_ = 0.0f;   // カメラ高さ（y を固定）
	float lobbyBaseTheta_ = 0.0f;   // 基準方位（θ）。yaw = θ + π
	float lobbyBasePitch_ = 0.0f;   // 基準ピッチ（上下角は固定）
	float swayPhase_ = 0.0f;   // スイングの位相
	float swaySpeed_ = 0.0625f;   // rad/sec（遅く左右に）
	float swayAmplitude_ = 0.03f;  // rad（左右振り幅 ≒ 6.9°）

	// オービット中心と高さ（見せたい地点）
	Vector3 orbitCenter_ = { 0.0f, 10.0f, 0.0f };

	// --- タイトル用カメラと簡易オービット ---
	Camera* camera_ = nullptr;
	float orbitRadius_ = 12.0f; // カメラの軌道半径
	float orbitSpeed_ = 0.0625f;   // カメラの軌道速度 rad/s
	float orbitAngle_ = 0.0f;   // カメラの現在の角度

	// --- 入力抑制 ---
	float minTitleSeconds_ = 1.0f;      // タイトルの最小表示時間
	float afterReturnCooldown_ = 0.75f;  // タイトルへ戻った直後の入力クールダウン
	float inputCooldownLeft_ = 0.0f;     // 現在のクールダウン残り

	// --- ロゴ演出 ---
	float logoAlpha_ = 0.0f;   // 0→1 でフェードイン
	float logoScale_ = 0.9f;   // 0.9→1.0 でふわっと拡大
	Vector2 logoBaseSize_{ 0,0 };    // 元画像のサイズ

	// 追加：ロゴの入退場タイミング
	float logoShowDelay_ = 0.5f;       // 戻った直後は0.5sロゴを出さない
	float logoShowDelayLeft_ = 0.0f;   // 残りディレイ
	float logoExitFade_ = 0.25f;       // ロビー遷移開始時のフェードアウト時間
	float logoExitFadeLeft_ = 0.0f;    // 残りフェード時間

	Vector2 xpBackBaseSize_{ 0,0 };         // XPバー元サイズ

	// ダミー数値（システム実装前の見た目用）
	int debugLevel_ = 7;
	int debugXP_ = 45;
	int debugXPNext_ = 175;
	int debugCoins_ = 1234;

	float blinkPhase_ = 0.0f;     // [クリックで開始] の点滅位相
	float blinkSpeed_ = 2.5f;     // 点滅速度（rad/sec）

private: /// ---------- メンバ変数 ----------///

	// フェード制御
	std::unique_ptr<FadeController> fadeController_ = nullptr;
	bool requestChange_ = false; // シーン切り替え要求

	float timer_ = 0.0f;   // シーン開始からの経過時間
};

