#pragma once
#include <BaseScene.h>
#include <Sprite.h>
#include <Object3D.h>
#include <SkyBox.h>
#include <FadeController.h>

#include "CollisionManager.h"

#include "Player.h"
#include <AnimationModel.h>

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

	// Debug用更新処理
	void UpdateDebug();

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

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Player> player_ = nullptr; // プレイヤー

};
