#pragma once
#include <Sprite.h>
#include <Object3D.h>
#include <WavLoader.h>
#include <MP3Loader.h>
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include <BaseScene.h>
#include <SkyBox.h>

#include "CollisionManager.h"

#include "Player.h"
#include "Enemy.h"
#include "Boss.h"
#include "FpsCamera.h"
#include "Crosshair.h"
#include "HUDManager.h"
#include "ResultManager.h"
#include "ItemManager.h"
#include "LevelObjectManager.h"
#include "ItemDropTable.h"

#include "AnimationModel.h"


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
private: /// ---------- 構造体 ---------- ///

	// スポーンリクエスト
	struct SpawnRequest
	{
		Vector3 position; // スポーン位置
		float timeLeft; // スポーンまでの残り時間
	};
	std::vector<SpawnRequest> spawnRequests_; // スポーンリクエストのリスト

	bool bossSpawned_ = false; // スポーン中かどうか
	Vector3 bossSpawnPosition_ = { 0.0f, 0.0f, 50.0f }; // ボスのスポーン位置

	// ウェーブ設定
	struct WaveConfig
	{
		int totalEnemies; // このウェーブで出現する敵の総数
		int batchSize; // 一度にスポーンする敵の数
		float spawnInterval; // バッチ間のスポーン間隔（秒）
		float batchInterval; // ウェーブ間のインターバル（秒）
	};

	std::vector<Vector3> enemySpawnPoints_;   // スポーン地点（3〜6個くらい）
	std::vector<WaveConfig> waves_;           // ステージ構成
	size_t waveIndex_ = 0;                    // 現在のウェーブ番号
	int enemiesToSpawn_ = 0;                  // このウェーブで未スポーンの残数
	int aliveEnemies_ = 0;                    // 現在生存中
	float spawnTimer_ = 0.0f;                 // スポーンタイマー
	int batchLeftInThisWave_ = 0;             // このウェーブで残っているバッチ数
	int batchRemainder_ = 0;                  // 端数（最後のバッチの体数）
	float batchCooldown_ = 0.0f;              // 次のバッチまでの待機
	int spawnedInThisBatch_ = 0;  // ★ 追加：同一バッチ内で何体出したか

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

	// 衝突判定と応答
	void CheckAllCollisions();

	void UpdatePlaying();
	void UpdatePaused();
	void UpdateResult();

	// 管理関数
	void InitWaves();
	void BeginWave(size_t idx);
	void UpdateWaveSpawner(float dt);
	void SpawnOneEnemy(const Vector3& pos);
	int  CountAliveEnemies() const;

	// HUD反映
	void UpdateHudWaveInfo();

	// ---- スポーン点を円周上に作る ----
	Vector3 RandomSpawnPointAroundPlayer(float minR, float maxR);

	void    RebuildSpawnPointsAroundPlayer(int count, float minR, float maxR);

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;

	GameState gameState_ = GameState::Playing; // ゲームの状態

	// 3Dオブジェクト
	std::unique_ptr<Object3D> terrein_ = nullptr; // 地形オブジェクト

	std::unique_ptr<Player> player_ = nullptr; // プレイヤーオブジェクト
	std::vector<std::unique_ptr<Enemy>> enemies_; // 敵オブジェクトのリスト
	std::unique_ptr<Boss> boss_ = nullptr;

	std::unique_ptr<AnimationModel> dModel_;

	std::unique_ptr<FpsCamera> fpsCamera_ = nullptr; // カメラオブジェクト
	std::unique_ptr<Crosshair> crosshair_ = nullptr; // クロスヘアオブジェクト

	std::unique_ptr<HUDManager> hudManager_ = nullptr; // HUDマネージャー
	std::unique_ptr<ResultManager> resultManager_ = nullptr; // 結果マネージャー

	// 仮のアイテムリスト
	std::unique_ptr<ItemManager> itemManager_ = nullptr;
	ItemDropTable dropTable_;

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー

	// アニメーションモデル
	std::unique_ptr<AnimationModel> animationModel_ = nullptr;

	std::unique_ptr<SkyBox> skyBox_ = nullptr; // スカイボックス

	std::unique_ptr<LevelObjectManager> levelObjectManager_ = nullptr; // レベルオブジェクトマネージャー

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;

	bool isPaused_ = false; // ポーズ中かどうか

	Vector2 scorePos = { 600.0f, 20.0f };
	Vector2 killPos = { 600.0f, 80.0f };

	bool isCharging = false;
	float chargeTimer = 0.0f;

	// ---- 半径の既定値 ----
	float spawnRadiusMin_ = 18.0f;

	float spawnRadiusMax_ = 25.0f;
};
