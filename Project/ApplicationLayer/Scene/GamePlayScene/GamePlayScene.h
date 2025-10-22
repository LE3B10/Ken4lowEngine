#pragma once
#include <BaseScene.h>
#include <Sprite.h>
#include <Object3D.h>
#include <SkyBox.h>
#include <FadeController.h>

#include "CollisionManager.h"

#include "Player.h"
#include "ModelParticle.h"
#include "BallisticEffect.h"
#include "Crosshair.h"
#include "Scarecrow.h" // 案山子クラス
#include "ItemManager.h"
#include "Stage.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;


/// -------------------------------------------------------------
///				　		ゲームの状態を管理する列挙型
/// -------------------------------------------------------------
enum class GameState
{
	Playing, // プレイ中
	Paused, // ポーズ中
	Result, // 結果画面
	CutScene // カットシーン中
};

/// -------------------------------------------------------------
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
private: /// ---------- 構造体 ---------- ///

	// カメラキーフレーム構造体
	struct CameraKeyFrame
	{
		float time;		  // 時間
		Vector3 position; // 位置
		Vector3 lookAt;	  // 注視点
	};

	// ヨー回転キーフレーム構造体
	struct YawKey
	{
		float time;
		float deg;
	};

	static float SmoothDampAngle(float current, float target,
		float& currentVelocity,
		float smoothTime, float deltaTime);

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

	void StartIntroCutscene();   // セットアップ

	bool UpdateIntroCutscene(float dt); // true=終わった

	float SampleYawDeg(float t) const; // 補間関数

	// 再生を最初からやり直す小関数
	void RestartIntroCutscene();

	// 衝突判定と応答
	void CheckAllCollisions();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;

	GameState gameState_ = GameState::Playing; // ゲームの状態

	std::unique_ptr<FadeController> fadeController_ = nullptr; // フェードコントローラー

	// 3Dオブジェクト
	std::unique_ptr<Object3D> terrein_ = nullptr; // 地形オブジェクト

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー

	std::unique_ptr<SkyBox> skyBox_ = nullptr; // スカイボックス

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;

	bool isPaused_ = false; // ポーズ中かどうか

	std::vector<CameraKeyFrame> introKeys_;
	float introLength_ = 0.0f;
	float introTime_ = 0.0f;
	bool  introDone_ = false;

	std::vector<YawKey> introYawKeys_; // Yawオフセット（度）
	float introYawRad_ = 0.0f;   // 現在のヨー（rad）
	float introYawVel_ = 0.0f;   // 角速度（rad/s）

	bool introLoop_ = false;        // ループ再生（ImGuiトグル）
	float introSmoothTime_ = 0.6f;  // Yawのスムーズ時間（ImGuiで可変）
	bool forceSnapFirstYaw_ = true; // 開始フレームで進行方向にヨーを合わせる

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Player> player_ = nullptr; // プレイヤー
	std::unique_ptr<Crosshair> crosshair_ = nullptr; // クロスヘア
	std::unique_ptr<BallisticEffect> ballisticEffect_ = nullptr; // 弾道エフェクト

	std::unique_ptr<Scarecrow> scarecrow_ = nullptr; // 案山子

	std::unique_ptr<ItemManager> itemManager_ = nullptr; // アイテムマネージャー

	std::unique_ptr<Stage> stage_ = nullptr; // ステージ
};
