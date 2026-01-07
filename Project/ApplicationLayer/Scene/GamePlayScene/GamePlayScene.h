#pragma once
#include <BaseScene.h>
#include <Sprite.h>
#include <Object3D.h>
#include <SkyBox.h>
#include "CollisionManager.h"
#include "Player.h"
#include "Enemy.h"
#include "BossEnemy.h"
#include "ModelParticle.h"
#include "BallisticEffect.h"
#include "Crosshair.h"
#include "ItemManager.h"
#include "LevelObjectManager.h"
#include <ItemDropTable.h>
#include "BaseOverlay.h"

#include "IGamePlaySceneState.h"

#include <array>
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
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
	// ★ 星３つ演出
	static constexpr int kClearStarCount = 3;
	static constexpr int kClearOptionCount = 3;

public: /// ---------- メンバ型 ---------- ///

	// ゲームの状態を管理する列挙型
	enum class State
	{
		SettingUp,  // ステージ読み込み前のセットアップ
		Loading,    // リソースロード

		CutScene,   // オープニング演出
		Playing,    // プレイ中
		Paused,     // ポーズ

		GameClear,  // クリア
		GameOver,   // ゲームオーバー
		Result,     // リザルト画面

		FadeOut,    // 次のシーンへフェードアウト
	};

	// ToDo: ゲームシーンにステートパターンを導入する予定

	struct ButtonRect
	{
		float x, y;      // 左上
		float w, h;      // サイズ
	};

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

	bool IsReadyToStartUncover() const override
	{
		return state_ != State::Loading; // GamePlayingState::Enterで Playing に変わる
	}

public: /// ---------- セッタ ー ---------- ///

	// シーン状態を変更
	void ChangeState(std::unique_ptr<IGamePlaySceneState> newState);

	// 状態をセット
	void SetState(State state) { state_ = state; }

public: /// ---------- ゲッター ---------- ///

	DirectXCommon* GetDirectXCommon() { return dxCommon_; }
	Input* GetInput() { return input_; }
	Player* GetPlayer() { return player_.get(); }
	std::unique_ptr<BossEnemy>& GetBoss() { return boss_; }
	std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return enemies_; }
	SkyBox* GetSkyBox() { return skyBox_.get(); }
	Crosshair* GetCrosshair() { return crosshair_.get(); }
	BallisticEffect* GetBallisticEffect() { return ballisticEffect_.get(); }
	ItemManager* GetItemManager() { return itemManager_.get(); }
	std::unique_ptr<LevelObjectManager>& GetLevelObjectManager() { return levelObjectManager_; }
	CollisionManager* GetCollisionManager() { return collisionManager_.get(); }
	State GetState() const { return state_; }
	ItemDropTable& GetItemDropTable() { return normalDropTable_; }
	std::vector<WaveConfig>* GetWaveConfigs() { return &currentStageConfig_.waves; }
	StageConfig& GetCurrentWaveConfig() { return currentStageConfig_; }
	ButtonRect& GetRetryRect() { return retryRect_; }
	ButtonRect& GetRetireRect() { return retireRect_; }

	int GetCurrentStageIndex() const { return currentStageIndex_; }
	void SetCurrentStageIndex(int index) { currentStageIndex_ = index; }

	// 半透明パネル
	std::unique_ptr<Sprite>& GetClearPanelSprite() { return clearPanelSprite_; }
	std::unique_ptr<Sprite>& GetClearTextSprite() { return clearTextSprite_; }

	std::unique_ptr<Sprite>& GetRetryButtonSprite() { return retryButtonSprite_; }
	std::unique_ptr<Sprite>& GetRetireButtonSprite() { return retireButtonSprite_; }

	int GetClearStarCount() const { return kClearStarCount; }
	std::array<std::unique_ptr<Sprite>, kClearStarCount>& GetClearStarSprites() { return clearStarSprites_; }
	std::array<float, kClearStarCount>& GetClearStarDelay() { return clearStarDelay_; }
	std::array<bool, kClearStarCount>& GetClearStarBurstPlayed() { return clearStarBurstPlayed_; }
	float GetClearStarPopDuration() const { return clearStarPopDuration_; }
	float GetClearStarBaseSize() const { return clearStarBaseSize_; }

	int GetClearOptionCount() const { return kClearOptionCount; }
	std::array<std::unique_ptr<Sprite>, kClearOptionCount>& GetClearOptionSprites() { return clearOptionSprites_; }
	std::array<ButtonRect, kClearOptionCount>& GetClearOptionRects() { return clearOptionRects_; }

	bool IsBossSpawned() const { return bossSpawned_; }
	void SetBossSpawned(bool spawned) { bossSpawned_ = spawned; }

	bool IsAllWavesCleared() const { return allWavesCleared_; }
	void SetAllWavesCleared(bool cleared) { allWavesCleared_ = cleared; }

	int GetCurrentWaveIndex() const { return currentWaveIndex_; }
	void SetCurrentWaveIndex(int index) { currentWaveIndex_ = index; }

	bool IsGameClearInputAccepted() const { return gameClearInputAccepted_; }
	void SetGameClearInputAccepted(bool accepted) { gameClearInputAccepted_ = accepted; }

	float GetGameClearTimer() const { return gameClearTimer_; }
	void SetGameClearTimer(float time) { gameClearTimer_ = time; }

	bool IsGameplayInitialized() const { return isGameplayInitialized_; }
	void SetGameplayInitialized(bool v) { isGameplayInitialized_ = v; }

	bool IsNextStageKeySpawned() const { return nextStageKeySpawned_; }
	void SetNextStageKeySpawned(bool spawned) { nextStageKeySpawned_ = spawned; }

public: /// ---------- ポーズオーバーレイ関連 ---------- ///

	BaseOverlay* GetPauseOverlay() { return pauseOverlay_.get(); }
	void SetPauseOverlay(std::unique_ptr<BaseOverlay> overlay) { pauseOverlay_ = std::move(overlay); }
	void ClearPauseOverlay() { pauseOverlay_.reset(); }

	bool IsPaused() const { return isPaused_; }
	void SetPaused(bool paused) { isPaused_ = paused; }

	bool IsDebugCamera() const { return isDebugCamera_; }

private: /// ---------- メンバ関数 ---------- ///

	// フェードオーバーレイの初期化
	void InitializeFadeOverlay();

	// アイテムの初期化
	void InitializeItems();

	// クリア演出スプライトの初期化
	void InitializeClearEffectSprites();

private: /// ---------- メンバ関数 ---------- ///

	// Debug用更新処理
	void UpdateDebug();

	// 衝突判定と応答
	void CheckAllCollisions();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Input* input_ = nullptr;

	State state_ = State::Playing; // ゲームの状態

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー

	std::unique_ptr<SkyBox> skyBox_ = nullptr; // スカイボックス

	// 現在のシーン状態
	std::unique_ptr<IGamePlaySceneState> currentState_ = nullptr;

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;

	bool isPaused_ = false; // ポーズ中かどうか

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Player> player_ = nullptr; // プレイヤー
	std::unique_ptr<Crosshair> crosshair_ = nullptr; // クロスヘア
	std::unique_ptr<BallisticEffect> ballisticEffect_ = nullptr; // 弾道エフェクト

	std::unique_ptr<ItemManager> itemManager_ = nullptr; // アイテムマネージャー

	std::unique_ptr<LevelObjectManager> levelObjectManager_ = nullptr; // レベルオブジェクトマネージャー

	std::unique_ptr<Sprite> retryButtonSprite_;   // 右下: リトライ
	std::unique_ptr<Sprite> retireButtonSprite_;  // 左下: リタイア(タイトルへ)

	// クリア演出用
	float gameClearTimer_ = 0.0f;        // 経過時間
	bool  gameClearInputAccepted_ = false; // 入力受付開始フラグ
	std::unique_ptr<Sprite> clearPanelSprite_; // 半透明パネル
	std::unique_ptr<Sprite> clearTextSprite_;  // 「STAGE CLEAR!」などの文字

	// ★ 星３つ演出
	std::array<std::unique_ptr<Sprite>, kClearStarCount> clearStarSprites_;
	std::array<float, kClearStarCount> clearStarDelay_;      // 何秒後に出るか
	std::array<bool, kClearStarCount> clearStarBurstPlayed_;
	float clearStarPopDuration_ = 0.25f;                     // 1 個のポップ時間
	float clearStarBaseSize_ = 96.0f;                        // 星の基本サイズ(px)

	ItemDropTable normalDropTable_;

	ButtonRect retryRect_;   // クリック判定用
	ButtonRect retireRect_;  // クリック判定用

	// ---------- GameClear 用 三択ボタン ----------
	// 0: もう一度同じステージ
	// 1: 次のステージへ
	// 2: セレクトシーンに戻る
	std::array<std::unique_ptr<Sprite>, kClearOptionCount> clearOptionSprites_;
	std::array<ButtonRect, kClearOptionCount> clearOptionRects_;

	// ボス
	std::unique_ptr<BossEnemy> boss_ = nullptr;
	std::vector<std::unique_ptr<Enemy>> enemies_;

	// ポーズオーバーレイ（ESC で開く）
	std::unique_ptr<BaseOverlay> pauseOverlay_;

	// Wave関連
	int  currentWaveIndex_ = 0;
	bool allWavesCleared_ = false;
	bool bossSpawned_ = false;

	// ステージ設定
	StageConfig currentStageConfig_{};
	int currentStageIndex_ = 0;

	// フェードオーバーレイ（黒スプライト）
	std::unique_ptr<Sprite> fadeSprite_;
	float fadeAlpha_ = 0.0f;

	// ゲームプレイ初期化フラグ
	bool isGameplayInitialized_ = false;

	bool nextStageKeySpawned_ = false;
};
