#pragma once
#include <BaseScene.h>
#include "PauseMenu.h"
#include <SkyBox.h>
#include "CollisionManager.h"
#include "CharacterWorld.h"
#include "BulletManager.h"
#include "HUDManager.h"
#include "WaveManager.h"
#include "ResultMenu.h"

#include "Stage.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }


/// -------------------------------------------------------------
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
private: /// ---------- 列挙型 ---------- ///

	enum class GameFlowState
	{
		Playing, // 通常プレイ中
		GameClear, // ゲームクリア
		GameOver, // ゲームオーバー
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// シャドウマップ描画
	void DrawShadowObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// Debug用更新処理
	void UpdateDebug();

	// 衝突判定更新処理
	void CollisionUpdate();

	// ポーズ制御
	void EnterPause();
	void ExitPause();
	void UpdatePaused(float deltaTime);

	void UpdateShadowLightViewProjection();

	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection);

	void SetupWaves();
	void EnterGameClear();
	void EnterGameOver();
	void UpdateResult(float deltaTime);
	void RestartGame();

	void EnterImGuiFreeze();
	void ExitImGuiFreeze();
	void UpdateImGuiFreeze();

private: /// ---------- メンバ変数 ---------- ///

	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;

	std::unique_ptr<PauseMenu> pauseMenu_ = nullptr; // ポーズメニュー
	std::unique_ptr<ResultMenu> resultMenu_ = nullptr; // 結果メニュー

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー
	std::unique_ptr<BulletManager> bulletManager_; // 弾丸マネージャー
	CharacterWorld characters_;

	std::unique_ptr<K4E::SkyBox> skyBox_ = nullptr; // スカイボックス

	std::unique_ptr<K4E::Stage> stage_ = nullptr; // ステージ（地形＋ワールドコリジョン）

	// ポーズ状態（ESCで切替）
	bool isPaused_ = false;

	std::unique_ptr<WaveManager> waveManager_ = nullptr;
	GameFlowState gameFlowState_ = GameFlowState::Playing;
	float resultInputCooldown_ = 0.0f;

	int prevWaveNumber_ = 0;
	bool prevWaveInProgress_ = false;
	bool prevAllWavesCleared_ = false;

private: /// ---------- 影用 ---------- ///

	K4E::Matrix4x4 shadowLightViewProjection_{}; // ライトのビュー射影行列

	K4E::Vector3 shadowLightDirection_ = { 0.3f, -1.0f, 0.2f, }; // 影用ライトの方向
	float shadowDistance_ = 50.0f; // 影の最大距離
	float shadowOrthoHalfWidth_ = 25.0f; // 影の直交投影の半幅
	float shadowOrthoHalfHeight_ = 25.0f; // 影の直交投影の半高さ
	float shadowNearZ_ = 0.1f; // 影の近距離
	float shadowFarZ_ = 120.0f; // 影の遠距離

private: /// ---------- HUD ---------- ///

	std::unique_ptr<HUDManager> hudManager_ = nullptr; // HUDマネージャー

private: /// ---------- 内部メンバ変数 ---------- ///

	// デバッグカメラのON/OFF用
	bool isDebugCamera_ = false;
	bool isLockedCursor_ = false;

	bool isImGuiFreeze_ = false;
};
