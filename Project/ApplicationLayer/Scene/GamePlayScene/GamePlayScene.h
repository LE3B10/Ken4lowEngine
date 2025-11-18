#pragma once
#include <BaseScene.h>
#include <Sprite.h>
#include <Object3D.h>
#include <SkyBox.h>
#include <FadeController.h>
#include "CollisionManager.h"
#include "Player.h"
#include "Enemy.h"
#include "ModelParticle.h"
#include "BallisticEffect.h"
#include "Crosshair.h"
#include "ItemManager.h"
#include "LevelObjectManager.h"
#include <ItemDropTable.h>
#include "BaseOverlay.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;

// 敵ウェーブ設定構造体
struct WaveConfig
{
	int enemyCount;
	float spawnRadius;

	// 個別スポーン位置
	// 空でなければこの配列を優先して使う
	std::vector<Vector3> spawnPositions;
};

// ステージごとの設定
struct StageConfig
{
	const char* levelJson;          // ステージ配置データ
	const char* levelModel;         // ステージモデル
	std::vector<WaveConfig> waves;  // ウェーブ構成
	float bossHp = 2500.0f;         // ボスHP
	Vector3 bossSpawnPos{ 0.0f, 2.5f, 0.0f }; // ボス出現位置

	// ステージごとの敵パラメータ
	float enemymaxHp = 100.0f;    // 敵最大HP
	float enemyWalkSpeed = 0.03f;  // 徘徊速度
	float enemyChaseSpeed = 0.08f;  // 追跡速度
	float enemyAttackDamage = 25.0f;  // 与ダメージ
	float enemyAttackCooldown = 0.8f;   // 攻撃間隔
	float enemyDetectRadius = 10.0f;  // 索敵範囲
};


/// -------------------------------------------------------------
///				　		ゲームの状態を管理する列挙型
/// -------------------------------------------------------------
enum class GameState
{
	Playing, // プレイ中
	Paused, // ポーズ中
	GameClear, // ゲームクリア
	GameOver, // ゲームオーバー
	CutScene, // カットシーン中
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

	// ステージクリア時の処理
	void OnStageClear();

	// 敵ウェーブ初期化処理
	void InitializeWaves();

	// 敵ウェーブ出現処理
	void SpawnWave(int waveIndex);

	// ボス出現処理
	void SpawnBoss();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;

	GameState gameState_ = GameState::Playing; // ゲームの状態

	std::unique_ptr<FadeController> fadeController_ = nullptr; // フェードコントローラー

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
	std::unique_ptr<Enemy> enemy_ = nullptr; // 敵キャラクター
	std::unique_ptr<Crosshair> crosshair_ = nullptr; // クロスヘア
	std::unique_ptr<BallisticEffect> ballisticEffect_ = nullptr; // 弾道エフェクト

	std::unique_ptr<ItemManager> itemManager_ = nullptr; // アイテムマネージャー

	std::unique_ptr<LevelObjectManager> levelObjectManager_ = nullptr; // レベルオブジェクトマネージャー

	std::unique_ptr<Sprite> retryButtonSprite_;   // 右下: リトライ
	std::unique_ptr<Sprite> retireButtonSprite_;  // 左下: リタイア(タイトルへ)

	ItemDropTable normalDropTable_;

	struct ButtonRect {
		float x, y;      // 左上
		float w, h;      // サイズ
	};
	ButtonRect retryRect_;   // クリック判定用
	ButtonRect retireRect_;  // クリック判定用

	std::vector<std::unique_ptr<Enemy>> enemies_;  // 通常敵

	// ボス
	std::unique_ptr<Enemy> boss_ = nullptr;

	// ポーズオーバーレイ（ESC で開く）
	std::unique_ptr<BaseOverlay> pauseOverlay_;

	// Wave関連
	std::vector<WaveConfig> waveConfigs_;
	int  currentWaveIndex_ = 0;
	bool allWavesCleared_ = false;
	bool bossSpawned_ = false;

	// ステージ設定
	StageConfig currentStageConfig_{};
	int currentStageIndex_ = 0;
};
