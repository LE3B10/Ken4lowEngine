#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <WavLoader.h>
#include <MP3Loader.h>
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include <BaseScene.h>

#include "CollisionManager.h"

#include "Player.h"
#include "Boss.h"
#include "FpsCamera.h"
#include "Crosshair.h"
#include "EnemyManager.h"
#include "HUDManager.h"
#include "ResultManager.h"
#include "ItemManager.h"

#include "AnimationModel.h"
#include "DummyModel.h"


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
	// 追加例：Menu, GameOver, Cutscene など
};

/// -------------------------------------------------------------
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
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

	// 衝突判定と応答
	void CheckAllCollisions();

	void UpdatePlaying();
	void UpdatePaused();
	void UpdateResult();

	void DrawPlaying();
	void DrawPaused();
	void DrawResult();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;

	GameState gameState_ = GameState::Playing; // ゲームの状態

	// 3Dオブジェクト
	std::unique_ptr<Object3D> terrein_ = nullptr; // 地形オブジェクト

	std::unique_ptr<Player> player_ = nullptr; // プレイヤーオブジェクト

	std::unique_ptr<Boss> boss_ = nullptr;

	std::unique_ptr<EnemyManager> enemyManager_ = nullptr;

	std::unique_ptr<DummyModel> dummyModel_ = nullptr;

	std::unique_ptr<AnimationModel> dModel_;

	std::unique_ptr<FpsCamera> fpsCamera_ = nullptr; // カメラオブジェクト
	std::unique_ptr<Crosshair> crosshair_ = nullptr; // クロスヘアオブジェクト

	std::unique_ptr<HUDManager> hudManager_ = nullptr; // HUDマネージャー
	std::unique_ptr<ResultManager> resultManager_ = nullptr; // 結果マネージャー

	// 仮のアイテムリスト
	std::unique_ptr<ItemManager> itemManager_ = nullptr;

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー

	// アニメーションモデル
	std::unique_ptr<AnimationModel> animationModel_ = nullptr;

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;

	bool isPaused_ = false; // ポーズ中かどうか

	Vector2 scorePos = { 600.0f, 20.0f };
	Vector2 killPos = { 600.0f, 80.0f };

	bool isCharging = false;
	float chargeTimer = 0.0f;
};
